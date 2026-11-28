#include "status_page.h"

#include "config.h"
#include "sdkconfig.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mdns_announce.h"
#include "p1_reader.h"
#include "status_json.h"
#include "tcp_server.h"
#include "version_gen.h"
#include "web.h"
#include "wifi.h"

_Static_assert(TCP_MAX_CLIENTS + 1 + HTTP_MAX_CLIENTS + HTTPD_OWN_SOCKETS <=
                   CONFIG_LWIP_MAX_SOCKETS,
               "readers, their listener and the web server must fit lwIP's sockets: raise "
               "CONFIG_LWIP_MAX_SOCKETS in sdkconfig.defaults");
_Static_assert(HTTP_TASK_PRIO < TCP_SERVER_TASK_PRIO && HTTP_TASK_PRIO < P1_READER_TASK_PRIO,
               "the web server must not outrank the telegram stream");

static const char *TAG = LOG_TAG_WEB;

/* Handlers run one at a time, on the server's task: one buffer, no allocation. */
static char s_json[STATUS_JSON_MAX];

static esp_err_t handle_page(httpd_req_t *req)
{
    return web_send_page(req, HTTPD_200, &WEB_PAGE_STATUS);
}

static esp_err_t handle_status(httpd_req_t *req)
{
    status_t status = {
        .firmware = FW_VERSION,
        .board = BOARD_NAME,
        .uptime_s = (uint32_t)(esp_timer_get_time() / US_PER_S),
        .readers = (unsigned)tcp_server_client_count(),
        .readers_max = TCP_MAX_CLIENTS,
    };
    status.has_rssi = wifi_get_rssi(&status.rssi);
    p1_reader_status(&status.p1);

    size_t len = status_json(&status, s_json, sizeof(s_json));
    if (len == 0) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Status too large");
    }
    return web_send(req, HTTPD_200, "application/json", s_json, len);
}

void status_page_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = HTTP_TASK_PRIO;
    config.max_open_sockets = HTTP_MAX_CLIENTS;
    config.lru_purge_enable = true; /* a new browser replaces the one idle longest */
    /* A browser that stops mid-request holds up the other browsers this long. */
    config.recv_wait_timeout = HTTP_WAIT_S;
    config.send_wait_timeout = HTTP_WAIT_S;

    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_page },
        { .uri = "/api/status", .method = HTTP_GET, .handler = handle_status },
    };
    esp_err_t err = web_start(&config, uris, sizeof(uris) / sizeof(uris[0]));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "status page not started: %s", esp_err_to_name(err));
        return;
    }
    mdns_announce_web();
    ESP_LOGI(TAG, "status page at http://%s.local (port %d)", HOSTNAME, HTTP_PORT);
}
