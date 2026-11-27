#include <stdio.h>

#include "config.h"
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns_announce.h"
#include "nvs_flash.h"
#include "p1_reader.h"
#include "recovery.h"
#include "setup_state.h"
#include "setup_button.h"
#include "setup_portal.h"
#include "status_led.h"
#include "tcp_server.h"
#include "version_gen.h"
#include "wifi.h"
#include "wifi_credentials.h"

#define SUPERVISOR_PERIOD_S 5
_Static_assert(SUPERVISOR_PERIOD_S < CONFIG_ESP_TASK_WDT_TIMEOUT_S,
               "supervisor must feed the watchdog");

static const char *TAG = LOG_TAG_MAIN;

/* Never boot-loops on NVS: erase and retry on any error, else run without it. */
static bool nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs init failed (%s), erasing", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs unavailable (%s): setup portal disabled", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void log_heartbeat(void)
{
    char rssi_txt[16] = "n/a";
    int rssi;
    if (wifi_get_rssi(&rssi)) {
        snprintf(rssi_txt, sizeof(rssi_txt), "%d dBm", rssi);
    }
    ESP_LOGI(TAG,
             "fw %s, uptime %llds, clients %d/%d, heap free %u (min %u, largest block %u), "
             "rssi %s",
             FW_VERSION, esp_timer_get_time() / US_PER_S, tcp_server_client_count(),
             TCP_MAX_CLIENTS, (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), rssi_txt);
}

/* Returns true once recovery is settled: connected, or setup requested. */
static bool check_recovery(const char *ssid)
{
    unsigned uptime_s = (unsigned)(esp_timer_get_time() / US_PER_S);
    setup_reason_t reason = recovery_check(wifi_ever_connected(), wifi_auth_failures(), uptime_s);
    if (reason != SETUP_REASON_NONE) {
        ESP_LOGW(TAG, "recovery (%s) for \"%s\": starting WiFi setup", setup_reason_name(reason),
                 ssid);
        setup_portal_request(reason, ssid);
    }
    return reason != SETUP_REASON_NONE || wifi_ever_connected();
}

/* Normal mode; restarts the device if WiFi stays down for OFFLINE_RESTART_S.
 * With `recovery_ssid`, offers WiFi setup when that network can't be used. */
static void supervise(const char *recovery_ssid)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int offline_s = 0;
    int no_client_s = 0;
    bool had_client = false;
    int since_heartbeat_s = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_PERIOD_S * 1000));
        esp_task_wdt_reset();

        if (recovery_ssid != NULL && check_recovery(recovery_ssid)) {
            recovery_ssid = NULL;
        }

        offline_s = wifi_is_connected() ? 0 : offline_s + SUPERVISOR_PERIOD_S;
        if (offline_s >= OFFLINE_RESTART_S) {
            ESP_LOGE(TAG, "no WiFi for %d s, restarting", offline_s);
            esp_restart();
        }

        /* "Connected" can hide a network that passes no traffic (e.g. a mesh node
         * without backhaul): readers that were here and stay away reveal it. */
        int clients = tcp_server_client_count();
        had_client = had_client || clients > 0;
        no_client_s = had_client && clients == 0 && wifi_is_connected()
                          ? no_client_s + SUPERVISOR_PERIOD_S
                          : 0;
        if (no_client_s >= NO_CLIENT_RESTART_S) {
            ESP_LOGE(TAG, "no readers for %d s while online, restarting", no_client_s);
            esp_restart();
        }

        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        if (largest < MIN_HEAP_BLOCK) {
            ESP_LOGE(TAG, "heap exhausted (largest block %u), restarting", (unsigned)largest);
            esp_restart();
        }

        since_heartbeat_s += SUPERVISOR_PERIOD_S;
        if (since_heartbeat_s >= HEARTBEAT_PERIOD_S) {
            since_heartbeat_s = 0;
            log_heartbeat();
        }
    }
}

/* Piepr's own modules log at INFO. ESP-IDF's components start at warnings (see
 * sdkconfig.defaults); the web server's stay at errors, because phones that
 * probe for internet drop connections all the time. */
static void set_log_levels(void)
{
    static const char *const OWN[] = LOG_TAGS;
    for (size_t i = 0; i < sizeof(OWN) / sizeof(OWN[0]); i++) {
        esp_log_level_set(OWN[i], ESP_LOG_INFO);
    }
    static const char *const WEB_SERVER[] = { "httpd_txrx", "httpd_parse", "httpd_sess",
                                              "httpd_uri" };
    for (size_t i = 0; i < sizeof(WEB_SERVER) / sizeof(WEB_SERVER[0]); i++) {
        esp_log_level_set(WEB_SERVER[i], ESP_LOG_ERROR);
    }
}

/* Why the device started, in a word; `*trouble` when it wasn't on purpose. */
static const char *start_reason(esp_reset_reason_t reason, bool *trouble)
{
    *trouble = true;
    switch (reason) {
    case ESP_RST_PANIC:
        return "a crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
        return "a hang (watchdog)";
    case ESP_RST_BROWNOUT:
        return "a power dip";
    default:
        break;
    }
    *trouble = false;
    switch (reason) {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_SW:
        return "a restart";
    case ESP_RST_USB:
        return "a reset over USB";
    default:
        return "a reset";
    }
}

void app_main(void)
{
    set_log_levels();
    esp_reset_reason_t reset = esp_reset_reason();
    bool trouble;
    const char *why = start_reason(reset, &trouble);
    ESP_LOG_LEVEL(trouble ? ESP_LOG_WARN : ESP_LOG_INFO, TAG,
                  "piepr %s starting after %s (reset reason %d)", FW_VERSION, why, (int)reset);

    bool nvs_ok = nvs_init();
    bool setup_requested = setup_portal_requested();
    wifi_credentials_t creds;
    bool has_wifi = wifi_credentials_load(nvs_ok, &creds);
    status_led_init();

    /* The setup portal needs NVS to store what the user enters. */
    if (nvs_ok && (setup_requested || !has_wifi)) {
        setup_portal_run(has_wifi);
    }

    /* Without NVS this runs unconnected; the supervisor then restarts every
     * OFFLINE_RESTART_S to retry NVS. */
    wifi_start(nvs_ok, has_wifi ? &creds : NULL);
    mdns_announce_start();
    tcp_server_start();
    p1_reader_start();
    if (nvs_ok) {
        setup_button_init();
    }
    /* Recovery needs NVS (to store what the user enters) and a network to give up on. */
    supervise(nvs_ok && has_wifi ? creds.ssid : NULL);
}
