#include "wifi.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "channel_pick.h"
#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "setup_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "secrets_gen.h"
#include "wifi_reason.h"

_Static_assert(WIFI_SSID_MAX_LEN <= MAX_SSID_LEN && WIFI_PASS_MAX_LEN < MAX_PASSPHRASE_LEN,
               "wifi_form limits must fit the ESP-IDF config fields");
_Static_assert(sizeof(SETUP_PASS) - 1 >= WIFI_PASS_MIN_LEN &&
                   sizeof(SETUP_PASS) - 1 <= WIFI_PASS_MAX_LEN,
               "setup_pass in secrets.ini must be 8-63 bytes");

_Static_assert(WIFI_REASON_TEXT_AUTH_EXPIRE == WIFI_REASON_AUTH_EXPIRE &&
                   WIFI_REASON_TEXT_ASSOC_LEAVE == WIFI_REASON_ASSOC_LEAVE &&
                   WIFI_REASON_TEXT_4WAY_TIMEOUT == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT &&
                   WIFI_REASON_TEXT_BEACON_TIMEOUT == WIFI_REASON_BEACON_TIMEOUT &&
                   WIFI_REASON_TEXT_NO_AP_FOUND == WIFI_REASON_NO_AP_FOUND &&
                   WIFI_REASON_TEXT_AUTH_FAIL == WIFI_REASON_AUTH_FAIL &&
                   WIFI_REASON_TEXT_ASSOC_FAIL == WIFI_REASON_ASSOC_FAIL &&
                   WIFI_REASON_TEXT_HANDSHAKE_TIMEOUT == WIFI_REASON_HANDSHAKE_TIMEOUT &&
                   WIFI_REASON_TEXT_CONNECTION_FAIL == WIFI_REASON_CONNECTION_FAIL,
               "wifi_reason.h must use ESP-IDF's reason numbers");

#define BACKOFF_MIN_US (1 * US_PER_S)
#define BACKOFF_MAX_US (30 * US_PER_S)
#define SCAN_ATTEMPTS  5
#define SCAN_RETRY_MS  1000 /* the driver refuses a scan while it is busy */
#define SCAN_ROUNDS    3    /* averaged: one weak moment doesn't decide the order */

static const char *TAG = LOG_TAG_NET;

static atomic_bool s_connected = false;
static atomic_bool s_ever_connected = false;
/* Consecutive auth-type disconnects; reset by any other outcome. */
static atomic_uint s_auth_failures;
static esp_timer_handle_t s_reconnect_timer;
static _Atomic int64_t s_backoff_us = BACKOFF_MIN_US; /* event task + esp_timer task */

/* Returns the seconds until the next attempt, for the caller's log line. */
static int schedule_reconnect(void)
{
    int64_t delay = atomic_load(&s_backoff_us);
    atomic_store(&s_backoff_us, delay * 2 > BACKOFF_MAX_US ? BACKOFF_MAX_US : delay * 2);

    esp_timer_stop(s_reconnect_timer);
    esp_err_t err = esp_timer_start_once(s_reconnect_timer, (uint64_t)delay);
    if (err != ESP_OK) {
        /* The supervisor restarts the device if WiFi stays down. */
        ESP_LOGE(TAG, "reconnect timer failed: %s", esp_err_to_name(err));
    }
    return (int)(delay / US_PER_S);
}

/* A rejected attempt produces no DISCONNECTED event: without this retry the
 * reconnect chain would stop for good. */
static void connect_or_retry(void)
{
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect failed: %s; retrying in %d s", esp_err_to_name(err),
                 schedule_reconnect());
    }
}

static void reconnect_cb(void *arg)
{
    (void)arg;
    connect_or_retry();
}

/* Reasons that mean "the network was found but rejected us": a wrong password,
 * or a link too weak to finish the handshake. */
static bool is_auth_failure(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_802_1X_AUTH_FAILED:
        return true;
    default:
        return false;
    }
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        connect_or_retry();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        atomic_store(&s_connected, false);
        if (is_auth_failure(d->reason)) {
            atomic_fetch_add(&s_auth_failures, 1);
        } else {
            atomic_store(&s_auth_failures, 0);
        }
        const char *why = wifi_reason_text(d->reason);
        ESP_LOGW(TAG, "disconnected (reason %d%s%s); retrying in %d s", d->reason,
                 why != NULL ? ": " : "", why != NULL ? why : "", schedule_reconnect());
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        atomic_store(&s_connected, true);
        atomic_store(&s_ever_connected, true);
        atomic_store(&s_auth_failures, 0);
        atomic_store(&s_backoff_us, BACKOFF_MIN_US);
        int rssi = 0;
        esp_wifi_sta_get_rssi(&rssi);
        ESP_LOGI(TAG, "connected, ip=" IPSTR ", signal %d dBm", IP2STR(&e->ip_info.ip), rssi);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        atomic_store(&s_connected, false);
        ESP_LOGW(TAG, "lost IP address, reconnecting");
        esp_wifi_disconnect(); /* the DISCONNECTED event starts a fresh connect */
    }
}

static void init_driver(bool nvs_available)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    if (!nvs_available) {
        init_config.nvs_enable = 0;
    }
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    /* No copy of the credentials in the driver's NVS, and no flash writes at
     * runtime (they stall the UART ISR's flash cache). */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

void wifi_start(bool nvs_available, const wifi_credentials_t *creds)
{
    init_driver(nvs_available);
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, HOSTNAME));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (creds != NULL) {
        const esp_timer_create_args_t timer_args = { .callback = reconnect_cb,
                                                     .name = "wifi_reconnect" };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

        wifi_config_t sta_config = {
            .sta = {
                .threshold.authmode = creds->pass[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK,
                /* Strongest AP for the SSID (mesh/repeaters), not the first found. */
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            },
        };
        memcpy(sta_config.sta.ssid, creds->ssid, strlen(creds->ssid));
        memcpy(sta_config.sta.password, creds->pass, strlen(creds->pass));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, on_event, NULL));
        ESP_LOGI(TAG, "connecting to \"%s\"", creds->ssid);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    /* USB powered: always listening gives predictable latency and avoids AP
     * power-save quirks on a marginal link (~0.3 W). */
    esp_wifi_set_ps(WIFI_PS_NONE);
}

bool wifi_is_connected(void)
{
    return atomic_load(&s_connected);
}

bool wifi_ever_connected(void)
{
    return atomic_load(&s_ever_connected);
}

unsigned wifi_auth_failures(void)
{
    return atomic_load(&s_auth_failures);
}

bool wifi_get_rssi(int *rssi)
{
    return wifi_is_connected() && esp_wifi_sta_get_rssi(rssi) == ESP_OK;
}

static esp_err_t scan_once(void)
{
    const wifi_scan_config_t scan = { .show_hidden = true };
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < SCAN_ATTEMPTS && err != ESP_OK; attempt++) {
        if (attempt > 0) {
            vTaskDelay(pdMS_TO_TICKS(SCAN_RETRY_MS));
        }
        err = esp_wifi_scan_start(&scan, true);
    }
    return err;
}

bool wifi_survey_networks(setup_network_t *nets, size_t *count)
{
    setup_survey_entry_t *entries = malloc(SETUP_SURVEY_MAX * sizeof(*entries));
    if (entries == NULL) {
        ESP_LOGE(TAG, "no memory for the network survey");
        return false;
    }
    size_t surveyed = 0;
    unsigned scans = 0;
    for (uint8_t round = 0; round < SCAN_ROUNDS; round++) {
        if (scan_once() != ESP_OK) {
            continue;
        }
        scans++;
        wifi_ap_record_t record;
        while (esp_wifi_scan_get_ap_record(&record) == ESP_OK) {
            const char *ssid = (const char *)record.ssid;
            setup_survey_add(entries, &surveyed, SETUP_SURVEY_MAX, round, ssid,
                             strnlen(ssid, sizeof(record.ssid)), record.rssi,
                             record.authmode != WIFI_AUTH_OPEN);
        }
        esp_wifi_clear_ap_list();
    }
    if (scans > 0) {
        *count = 0;
        setup_survey_result(entries, surveyed, nets, count);
        ESP_LOGI(TAG, "%u networks nearby, %u listed", (unsigned)surveyed, (unsigned)*count);
    } else {
        ESP_LOGE(TAG, "network scan failed");
    }
    free(entries);
    return scans > 0;
}

/* The quietest channel from one scan (station started). */
static uint8_t pick_channel(void)
{
    channel_pick_ap_t aps[SETUP_SCAN_MAX_APS];
    size_t seen = 0;
    if (scan_once() == ESP_OK) {
        wifi_ap_record_t record;
        while (seen < SETUP_SCAN_MAX_APS && esp_wifi_scan_get_ap_record(&record) == ESP_OK) {
            aps[seen++] = (channel_pick_ap_t){ .channel = record.primary, .rssi = record.rssi };
        }
        esp_wifi_clear_ap_list();
    }
    return channel_pick_quietest(aps, seen);
}

esp_netif_t *wifi_start_setup_ap(uint8_t *channel)
{
    init_driver(true);
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    *channel = pick_channel();
    ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = sizeof(SETUP_AP_SSID) - 1,
            .channel = *channel,
            .max_connection = SETUP_AP_MAX_STATIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    memcpy(ap_config.ap.ssid, SETUP_AP_SSID, sizeof(SETUP_AP_SSID) - 1);
    memcpy(ap_config.ap.password, SETUP_PASS, sizeof(SETUP_PASS) - 1);

    /* The hotspot is fully configured before it starts, in ESP-IDF's example
     * order (mode, bandwidth, config with explicit channel): changing these on a
     * running hotspot left it invisible or unjoinable. The idle station serves the
     * page's scans. 20 MHz: the default 40 MHz in a crowded 2.4 GHz band was not
     * joinable for an Android phone. */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_bandwidths_t bandwidth = { .ghz_2g = WIFI_BW20 };
    ESP_ERROR_CHECK(esp_wifi_set_bandwidths(WIFI_IF_AP, &bandwidth));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    return netif;
}
