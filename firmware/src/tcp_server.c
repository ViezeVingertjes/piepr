#include "tcp_server.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

/*
 * Robustness rules:
 *  - Fixed client table, no allocation per client. Only the server task
 *    writes .fd and closes sockets; broadcast() only sends and marks a slot
 *    .send_failed, both under s_lock.
 *  - Sockets are closed with SO_LINGER {1,0} (abort/RST): close() never waits
 *    for a FIN to be queued, and it is never called while holding s_lock.
 *  - Clients only read. Any payload a client sends gets it disconnected (stops
 *    a sender from starving this task); a half-close (FIN) just stops reading.
 *  - accept/select errors never end the loop: log, back off, retry; on select
 *    errors all sockets are recreated.
 *  - The task is watched by the task watchdog (select timeout 1 s).
 */

#define SELECT_TIMEOUT_MS 1000
_Static_assert(SELECT_TIMEOUT_MS < CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000, "must feed the watchdog");
#define ERROR_BACKOFF_MS     1000
#define LISTEN_BACKLOG       2
#define KEEPALIVE_IDLE_S     30
#define KEEPALIVE_INTERVAL_S 10
#define KEEPALIVE_PROBES     3 /* dead client detected after ~60 s */

_Static_assert(TCP_MAX_CLIENTS + 1 <= CONFIG_LWIP_MAX_SOCKETS, "clients + listener must fit lwIP's sockets");

static const char *TAG = "tcp";

typedef struct {
    int fd;              /* -1 = free */
    bool send_failed;    /* a broadcast could not be sent in full; server task closes it */
    int send_result;     /* bytes of that broadcast sent, or -errno */
    bool half_closed;    /* client sent FIN: keep sending, stop reading */
    uint32_t accept_seq; /* order of acceptance; the lowest is the oldest */
} client_t;

static client_t s_clients[TCP_MAX_CLIENTS];
static SemaphoreHandle_t s_lock;
static int s_listen_fd = -1;
static uint32_t s_accept_seq; /* server task only */

static int count_active_locked(void)
{
    int n = 0;
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_clients[i].fd >= 0 && !s_clients[i].send_failed) {
            n++;
        }
    }
    return n;
}

/* Server task only. Frees the slot under the lock, closes outside it. */
static void close_client(int i, const char *why)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int fd = s_clients[i].fd;
    s_clients[i] = (client_t){ .fd = -1 };
    int n = count_active_locked();
    xSemaphoreGive(s_lock);

    close(fd); /* SO_LINGER {1,0}: aborts immediately */
    ESP_LOGI(TAG, "client %s (%d/%d)", why, n, TCP_MAX_CLIENTS);
}

static void close_all_clients(const char *why)
{
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_clients[i].fd >= 0) {
            close_client(i, why);
        }
    }
}

static void close_listener(void)
{
    if (s_listen_fd >= 0) {
        close(s_listen_fd);
        s_listen_fd = -1;
    }
}

static bool open_listener(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        return false;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, LISTEN_BACKLOG) != 0) {
        ESP_LOGE(TAG, "bind/listen on %d failed: errno %d", TCP_PORT, errno);
        close(fd);
        return false;
    }
    s_listen_fd = fd;
    ESP_LOGI(TAG, "listening on port %d (max %d clients)", TCP_PORT, TCP_MAX_CLIENTS);
    return true;
}

static void configure_client(int fd)
{
    int on = 1;
    int idle = KEEPALIVE_IDLE_S, interval = KEEPALIVE_INTERVAL_S, probes = KEEPALIVE_PROBES;
    const struct linger abort_on_close = { .l_onoff = 1, .l_linger = 0 };
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &abort_on_close, sizeof(abort_on_close));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

/* Server task only (reads .fd without the lock). */
static int free_slot(void)
{
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_clients[i].fd < 0) {
            return i;
        }
    }
    return -1;
}

/* Server task only; all slots in use. Unsigned subtraction keeps the order
 * correct when s_accept_seq wraps around. */
static int oldest_slot(void)
{
    int oldest = 0;
    for (int i = 1; i < TCP_MAX_CLIENTS; i++) {
        if ((int32_t)(s_clients[i].accept_seq - s_clients[oldest].accept_seq) < 0) {
            oldest = i;
        }
    }
    return oldest;
}

static void handle_accept(void)
{
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    int fd = accept(s_listen_fd, (struct sockaddr *)&peer, &plen);
    if (fd < 0) {
        int e = errno;
        ESP_LOGW(TAG, "accept() failed: errno %d", e);
        if (e == EBADF || e == ENOTSOCK || e == EINVAL) {
            close_listener(); /* recreated on the next loop */
        } else {
            vTaskDelay(pdMS_TO_TICKS(ERROR_BACKOFF_MS)); /* e.g. out of sockets */
        }
        return;
    }
    configure_client(fd);

    char ip[INET_ADDRSTRLEN];
    inet_ntoa_r(peer.sin_addr, ip, sizeof(ip));

    /* Full: make room by evicting the oldest client, so leftover readers can't
     * lock out a (re)connecting Home Assistant. */
    int slot = free_slot();
    if (slot < 0) {
        slot = oldest_slot();
        close_client(slot, "evicted (oldest)");
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_clients[slot] = (client_t){ .fd = fd, .accept_seq = s_accept_seq++ };
    int n = count_active_locked();
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "client %s connected (%d/%d)", ip, n, TCP_MAX_CLIENTS);
}

static void handle_client_readable(int i)
{
    uint8_t byte;
    int r = recv(s_clients[i].fd, &byte, 1, MSG_DONTWAIT);
    if (r > 0) {
        close_client(i, "sent data (not allowed), disconnected");
    } else if (r == 0) {
        /* Half-close: the client can still read. Linux doesn't answer keepalive
         * probes after a half-close, so keepalive drops it while the meter is
         * silent - intended, or a dead one would keep its slot. */
        s_clients[i].half_closed = true;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        close_client(i, "disconnected");
    }
}

static void server_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for (;;) {
        esp_task_wdt_reset();

        for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
            if (s_clients[i].fd >= 0 && s_clients[i].send_failed) {
                char why[48];
                int result = s_clients[i].send_result;
                snprintf(why, sizeof(why),
                         result < 0 ? "dropped (send error %d)" : "dropped (too slow, sent %d)",
                         result < 0 ? -result : result);
                close_client(i, why);
            }
        }

        if (s_listen_fd < 0 && !open_listener()) {
            vTaskDelay(pdMS_TO_TICKS(ERROR_BACKOFF_MS));
            continue;
        }

        /* rfds: payload/FIN/errors; efds: errors (RST, keepalive timeout) for
         * half-closed clients too, which are no longer polled for reading. */
        fd_set rfds, efds;
        FD_ZERO(&rfds);
        FD_ZERO(&efds);
        FD_SET(s_listen_fd, &rfds);
        int maxfd = s_listen_fd;
        for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
            int fd = s_clients[i].fd;
            if (fd < 0) {
                continue;
            }
            FD_SET(fd, &efds);
            if (!s_clients[i].half_closed) {
                FD_SET(fd, &rfds);
            }
            if (fd > maxfd) {
                maxfd = fd;
            }
        }

        struct timeval tv = { .tv_sec = SELECT_TIMEOUT_MS / 1000,
                              .tv_usec = (SELECT_TIMEOUT_MS % 1000) * 1000 };
        int r = select(maxfd + 1, &rfds, NULL, &efds, &tv);
        if (r < 0) {
            if (errno != EINTR) {
                /* Can't tell which fd is bad: start over with all sockets. */
                ESP_LOGW(TAG, "select() failed: errno %d, resetting all sockets", errno);
                close_all_clients("disconnected (select error)");
                close_listener();
                vTaskDelay(pdMS_TO_TICKS(ERROR_BACKOFF_MS));
            }
            continue;
        }
        if (r == 0) {
            continue;
        }

        /* Clients first: handle_accept() may fill a slot whose new fd is not in rfds. */
        for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
            int fd = s_clients[i].fd;
            if (fd >= 0 && FD_ISSET(fd, &efds)) {
                int so_error = 0;
                socklen_t optlen = sizeof(so_error);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &optlen);
                ESP_LOGW(TAG, "client slot %d connection error (errno %d)", i, so_error);
                close_client(i, "disconnected (connection error)");
            } else if (fd >= 0 && FD_ISSET(fd, &rfds)) {
                handle_client_readable(i);
            }
        }
        if (FD_ISSET(s_listen_fd, &rfds)) {
            handle_accept();
        }
    }
}

void tcp_server_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    configASSERT(s_lock);
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        s_clients[i] = (client_t){ .fd = -1 };
    }
    BaseType_t ok = xTaskCreate(server_task, "tcp_server", TCP_SERVER_TASK_STACK, NULL,
                                TCP_SERVER_TASK_PRIO, NULL);
    configASSERT(ok == pdPASS);
}

void tcp_server_broadcast(const uint8_t *data, size_t len)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (s_clients[i].fd < 0 || s_clients[i].send_failed) {
            continue;
        }
        int n = send(s_clients[i].fd, data, len, MSG_DONTWAIT);
        if (n != (int)len) {
            s_clients[i].send_failed = true; /* stream broken; the server task closes it */
            s_clients[i].send_result = n < 0 ? -errno : n;
        }
    }
    xSemaphoreGive(s_lock);
}

int tcp_server_client_count(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = count_active_locked();
    xSemaphoreGive(s_lock);
    return n;
}
