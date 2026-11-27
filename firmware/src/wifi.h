#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_netif.h"
#include "setup_types.h"
#include "wifi_form.h"

/* Station mode with hostname HOSTNAME; reconnects with a 1 -> 30 s back-off.
 * creds == NULL starts the station without connecting (no network known).
 * nvs_available = false runs without NVS (PHY calibration not cached). */
void wifi_start(bool nvs_available, const wifi_credentials_t *creds);

/* True while associated and holding an IP address. */
bool wifi_is_connected(void);

/* True once the station got an IP address since boot. */
bool wifi_ever_connected(void);

/* Consecutive disconnects with an authentication-type reason (wrong password or
 * a link too weak for the handshake); reset by any other outcome. */
unsigned wifi_auth_failures(void);

/* RSSI of the current AP in dBm; false when not connected. */
bool wifi_get_rssi(int *rssi);

/* Setup boot: scans a few times (~8 s, blocking) and fills `nets` with the
 * strongest usable networks by average signal (see setup_survey_result).
 * Returns false, leaving `nets` unchanged, if no scan succeeded. */
bool wifi_survey_networks(setup_network_t *nets, size_t *count);

/* Setup boot: scans once (~2 s) for the quietest channel, stored in `channel`,
 * then starts WiFi as the WPA2 setup hotspot (AP + idle station) on it. */
esp_netif_t *wifi_start_setup_ap(uint8_t *channel);
