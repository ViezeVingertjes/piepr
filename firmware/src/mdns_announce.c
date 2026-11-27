#include "mdns_announce.h"

#include "config.h"
#include "esp_log.h"
#include "mdns.h"

#define MDNS_INSTANCE_NAME "Piepr"
#define MDNS_SERVICE_TYPE  "_dsmr"
#define MDNS_PROTO         "_tcp"

static const char *TAG = "mdns";

void mdns_announce_start(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        /* Not fatal: the device still works via its IP address. */
        ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err));
        return;
    }
    err = mdns_hostname_set(HOSTNAME);
    if (err == ESP_OK) {
        err = mdns_instance_name_set(MDNS_INSTANCE_NAME);
    }
    if (err == ESP_OK) {
        err = mdns_service_add(NULL, MDNS_SERVICE_TYPE, MDNS_PROTO, TCP_PORT, NULL, 0);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setup failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "announcing %s.local, %s.%s port %d", HOSTNAME, MDNS_SERVICE_TYPE, MDNS_PROTO,
             TCP_PORT);
}
