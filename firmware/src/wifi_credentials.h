#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "wifi_form.h"

/* Credentials saved through the setup portal; false when none are saved. */
bool wifi_credentials_load(bool nvs_available, wifi_credentials_t *out);

/* Stores credentials in NVS; used from the next boot on. */
esp_err_t wifi_credentials_save(const wifi_credentials_t *creds);
