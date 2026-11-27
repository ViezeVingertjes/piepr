#include "wifi_credentials.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#define NVS_NAMESPACE "wifi_creds"
#define KEY_CREDS     "creds" /* one blob: name and password are written together */

static const char *TAG = "wifi_creds";

static bool load_saved(wifi_credentials_t *out)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    wifi_credentials_t saved;
    size_t size = sizeof(saved);
    bool ok = nvs_get_blob(nvs, KEY_CREDS, &saved, &size) == ESP_OK && size == sizeof(saved) &&
              memchr(saved.ssid, '\0', sizeof(saved.ssid)) != NULL &&
              memchr(saved.pass, '\0', sizeof(saved.pass)) != NULL && saved.ssid[0] != '\0';
    nvs_close(nvs);
    if (ok) {
        *out = saved;
    }
    return ok;
}

bool wifi_credentials_load(bool nvs_available, wifi_credentials_t *out)
{
    if (nvs_available && load_saved(out)) {
        ESP_LOGI(TAG, "using the saved WiFi network");
        return true;
    }
    ESP_LOGW(TAG, "no WiFi configured");
    return false;
}

esp_err_t wifi_credentials_save(const wifi_credentials_t *creds)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    wifi_credentials_t record = { 0 }; /* no stale bytes after the terminators */
    memcpy(record.ssid, creds->ssid, strnlen(creds->ssid, WIFI_SSID_MAX_LEN));
    memcpy(record.pass, creds->pass, strnlen(creds->pass, WIFI_PASS_MAX_LEN));
    err = nvs_set_blob(nvs, KEY_CREDS, &record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}
