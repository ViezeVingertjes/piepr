#include "setup_portal.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "captive_dns.h"
#include "config.h"
#include "dhcpserver/dhcpserver.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lang_pick.h"
#include "lwip/sockets.h"
#include "setup_state.h"
#include "status_led.h"
#include "wifi.h"
#include "wifi_credentials.h"
#include "wifi_form.h"

#define REQUEST_MAGIC         0x50315334u /* "P1S4": bump when setup_request_t changes */
#define SCAN_TASK_STACK       4096
#define SCAN_TASK_PRIO        2
#define DNS_PORT              53
#define DNS_TASK_STACK        (3072 + 2 * CAPTIVE_DNS_MAX_PACKET)
#define DNS_TASK_PRIO         4
#define DNS_ERROR_BACKOFF_MS  100
#define HTTP_MAX_SOCKETS      4 /* of LWIP_MAX_SOCKETS 10, leaving headroom */
#define MAX_FORM_BODY         512
#define MAX_RECV_TIMEOUTS     3
#define RESTART_AFTER_SAVE_MS 1000
#define TIMEOUT_CHECK_MS      1000
#define ACCEPT_LANGUAGE_MAX   96 /* the start of the header is what decides */
#define LANG_QUERY_MAX        16 /* "lang=nl"; a longer query is none of ours */

static const char *TAG = LOG_TAG_SETUP;

/* The setup pages of web/, gzipped, each in English and Dutch. The generated
 * file defines one localized_page_t per page: PAGE_setup, PAGE_saved, ... */
typedef struct {
    const uint8_t *data;
    size_t len;
} page_t;
typedef struct {
    page_t english;
    page_t dutch;
} localized_page_t;
#include "web_pages_gen.inc"

/* Carries a setup request across esp_restart() (lost on power-up/RST). */
typedef struct {
    uint32_t magic;
    uint8_t reason; /* setup_reason_t */
    char reason_ssid[WIFI_SSID_MAX_LEN + 1];
} setup_request_t;

static RTC_NOINIT_ATTR setup_request_t s_request;
static atomic_bool s_requested;
static _Atomic int64_t s_last_activity_us;

/* Setup boot state, allocated there so normal mode pays nothing for it. HTTP
 * handlers all run on the server's one task and only they set `scanning`, so
 * while it reads false the list is stable. */
typedef struct {
    uint8_t ip[4];
    char url[32];
    setup_network_t networks[SETUP_SCAN_LIST_MAX];
    size_t network_count;
    atomic_bool scanning;
} portal_t;

/* ---- Normal mode: request setup ---------------------------------------- */

void setup_portal_request(setup_reason_t reason, const char *ssid)
{
    if (atomic_exchange(&s_requested, true)) {
        return;
    }
    s_request.reason = (uint8_t)reason;
    snprintf(s_request.reason_ssid, sizeof(s_request.reason_ssid), "%s", ssid != NULL ? ssid : "");
    s_request.magic = REQUEST_MAGIC;
    ESP_LOGW(TAG, "restarting into setup mode");
    esp_restart();
}

bool setup_portal_requested(void)
{
    bool requested = esp_reset_reason() == ESP_RST_SW && s_request.magic == REQUEST_MAGIC;
    s_request.magic = 0;
    /* Without a request (power-up, unconfigured) the record holds random bytes. */
    if (!requested || s_request.reason > SETUP_REASON_AUTH) {
        s_request.reason = SETUP_REASON_NONE;
    }
    if (s_request.reason == SETUP_REASON_NONE) {
        s_request.reason_ssid[0] = '\0';
    }
    s_request.reason_ssid[sizeof(s_request.reason_ssid) - 1] = '\0';
    return requested;
}

/* ---- Setup boot: web form ---------------------------------------------- */

static esp_err_t send_response(httpd_req_t *req, const char *status, const char *type,
                               const void *body, size_t len)
{
    atomic_store(&s_last_activity_us, esp_timer_get_time());
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, (ssize_t)len);
}

/* The language chosen with the link on the page ("?lang=nl") wins; without one,
 * Dutch for a browser that asks for it (Accept-Language) and English otherwise. */
static bool wants_dutch(httpd_req_t *req)
{
    char query[LANG_QUERY_MAX] = { 0 };
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        lang_choice_t chosen = lang_from_query(query, strnlen(query, sizeof(query)));
        if (chosen != LANG_UNSET) {
            return chosen == LANG_DUTCH;
        }
    }
    char header[ACCEPT_LANGUAGE_MAX] = { 0 };
    esp_err_t err = httpd_req_get_hdr_value_str(req, "Accept-Language", header, sizeof(header));
    if (err != ESP_OK && err != ESP_ERR_HTTPD_RESULT_TRUNC) {
        return false;
    }
    return lang_prefers_dutch(header, strnlen(header, sizeof(header)));
}

static esp_err_t send_page(httpd_req_t *req, const char *status, const localized_page_t *pages)
{
    const page_t *page = wants_dutch(req) ? &pages->dutch : &pages->english;
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return send_response(req, status, "text/html; charset=utf-8", page->data, page->len);
}

static esp_err_t handle_state(httpd_req_t *req)
{
    char *json = malloc(SETUP_STATE_JSON_MAX);
    if (json == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    const portal_t *portal = req->user_ctx;
    bool scanning = atomic_load(&portal->scanning);
    size_t len = setup_state_json((setup_reason_t)s_request.reason, s_request.reason_ssid,
                                  portal->networks, scanning ? 0 : portal->network_count, scanning,
                                  json, SETUP_STATE_JSON_MAX);
    esp_err_t err =
        len > 0 ? send_response(req, HTTPD_200, "application/json", json, len)
                : httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "State too large");
    free(json);
    return err;
}

/* A failed survey keeps the previous list. */
static void scan_task(void *arg)
{
    portal_t *portal = arg;
    wifi_survey_networks(portal->networks, &portal->network_count);
    atomic_store(&portal->scanning, false);
    vTaskDelete(NULL);
}

/* Starts a scan (unless one is running); the page polls /api/state until
 * "scanning" is false. */
static esp_err_t handle_scan(httpd_req_t *req)
{
    portal_t *portal = req->user_ctx;
    if (!atomic_exchange(&portal->scanning, true) &&
        xTaskCreate(scan_task, "setup_scan", SCAN_TASK_STACK, portal, SCAN_TASK_PRIO, NULL) !=
            pdPASS) {
        atomic_store(&portal->scanning, false);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    return handle_state(req);
}

static esp_err_t handle_form(httpd_req_t *req)
{
    return send_page(req, HTTPD_200, &PAGE_setup);
}

static esp_err_t handle_save(httpd_req_t *req)
{
    char body[MAX_FORM_BODY];
    if (req->content_len > sizeof(body)) {
        return send_page(req, HTTPD_400, &PAGE_invalid);
    }
    size_t received = 0;
    int timeouts = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, body + received, req->content_len - received);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < MAX_RECV_TIMEOUTS) {
            continue;
        }
        if (r <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)r;
    }

    wifi_credentials_t creds;
    if (wifi_form_parse(body, received, &creds) != WIFI_FORM_OK) {
        return send_page(req, HTTPD_400, &PAGE_invalid);
    }
    esp_err_t err = wifi_credentials_save(&creds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "saving failed: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Saving failed");
    }
    ESP_LOGI(TAG, "saved WiFi settings for \"%s\", restarting", creds.ssid);
    send_page(req, HTTPD_200, &PAGE_saved);
    vTaskDelay(pdMS_TO_TICKS(RESTART_AFTER_SAVE_MS)); /* let the page reach the phone */
    esp_restart();
}

/* Phones probe URLs like /generate_204; a redirect makes them show the portal.
 * iOS only recognises a redirect that has a body. */
static esp_err_t handle_redirect(httpd_req_t *req)
{
    const portal_t *portal = req->user_ctx; /* phones probe in the background: no activity */
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", portal->url);
    return httpd_resp_sendstr(req, "Redirect to the setup page");
}

static void start_http_server(portal_t *portal)
{
    /* Phones that probe for internet drop connections all the time: the web
     * server's warnings about that are not worth a line in the log. */
    static const char *const SERVER_TAGS[] = { "httpd_txrx", "httpd_parse", "httpd_sess",
                                               "httpd_uri" };
    for (size_t i = 0; i < sizeof(SERVER_TAGS) / sizeof(SERVER_TAGS[0]); i++) {
        esp_log_level_set(SERVER_TAGS[i], ESP_LOG_ERROR);
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true; /* phones open many probe connections */
    config.max_open_sockets = HTTP_MAX_SOCKETS;

    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_form },
        { .uri = "/save", .method = HTTP_POST, .handler = handle_save },
        { .uri = "/api/state", .method = HTTP_GET, .handler = handle_state, .user_ctx = portal },
        { .uri = "/api/scan", .method = HTTP_POST, .handler = handle_scan, .user_ctx = portal },
        { .uri = "/*", .method = HTTP_GET, .handler = handle_redirect, .user_ctx = portal },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uris[i]));
    }
}

/* ---- Setup boot: captive portal plumbing -------------------------------- */

/* ESP-IDF's DHCP server offers no DNS server by default; without one a phone's
 * connectivity check never reaches our DNS, it decides "no internet" and
 * leaves. Offer our own address, plus the portal URL (option 114, RFC 8910). */
static void configure_dhcp(esp_netif_t *ap, const esp_netif_ip_info_t *ip, portal_t *portal)
{
    esp_netif_dns_info_t dns = { .ip = { .u_addr.ip4 = ip->ip, .type = ESP_IPADDR_TYPE_V4 } };
    dhcps_offer_t offer_dns = OFFER_DNS;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(ap));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &offer_dns, sizeof(offer_dns)));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI,
                                           portal->url, strlen(portal->url)));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));
}

static void dns_task(void *arg)
{
    const portal_t *portal = arg;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_port = htons(DNS_PORT),
                                .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "DNS socket failed: errno %d (portal still works via its IP)", errno);
        vTaskDelete(NULL);
        return;
    }

    uint8_t query[CAPTIVE_DNS_MAX_PACKET];
    uint8_t reply[CAPTIVE_DNS_MAX_PACKET];
    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int n = recvfrom(fd, query, sizeof(query), 0, (struct sockaddr *)&peer, &plen);
        if (n < 0) {
            vTaskDelay(pdMS_TO_TICKS(DNS_ERROR_BACKOFF_MS)); /* never spin on a broken socket */
            continue;
        }
        size_t len = captive_dns_reply(query, (size_t)n, portal->ip, reply, sizeof(reply));
        if (len > 0) {
            sendto(fd, reply, len, 0, (struct sockaddr *)&peer, plen);
        }
    }
}

/* The WiFi driver's own "station join/leave" lines are not shown; these are. */
static void on_hotspot_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "a device joined the hotspot");
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "a device left the hotspot");
    }
}

void setup_portal_run(bool has_wifi)
{
    status_led_show_setup();
    portal_t *portal = calloc(1, sizeof(*portal)); /* never freed: this never returns */
    configASSERT(portal != NULL);
    uint8_t channel;
    esp_netif_t *ap = wifi_start_setup_ap(&channel);
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_hotspot_event, NULL));
    esp_netif_ip_info_t ip;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(ap, &ip));
    memcpy(portal->ip, &ip.ip.addr, sizeof(portal->ip));
    snprintf(portal->url, sizeof(portal->url), "http://" IPSTR "/", IP2STR(&ip.ip));

    configure_dhcp(ap, &ip, portal);
    start_http_server(portal);
    BaseType_t ok = xTaskCreate(dns_task, "setup_dns", DNS_TASK_STACK, portal, DNS_TASK_PRIO, NULL);
    configASSERT(ok == pdPASS);
    ESP_LOGI(TAG, "access point \"%s\" on channel %u, open %s", SETUP_AP_SSID, channel,
             portal->url);

    /* Without a known network there is nothing to return to: wait for a save.
     * Otherwise return to normal mode after the window, counted from the last
     * activity; shorter after recovery, so the saved network is retried soon. */
    int64_t window_us =
        (s_request.reason != SETUP_REASON_NONE ? RECOVERY_SETUP_WINDOW_S : SETUP_TIMEOUT_S) *
        US_PER_S;
    atomic_store(&s_last_activity_us, esp_timer_get_time());
    while (!has_wifi || esp_timer_get_time() - atomic_load(&s_last_activity_us) < window_us) {
        vTaskDelay(pdMS_TO_TICKS(TIMEOUT_CHECK_MS));
    }
    ESP_LOGW(TAG, "setup timeout, restarting");
    esp_restart();
}
