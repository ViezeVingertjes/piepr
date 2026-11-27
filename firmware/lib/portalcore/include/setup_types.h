#pragma once

/* Types shared by the setup portal, the WiFi scan and the recovery rules. */

#include <stdbool.h>
#include <stdint.h>

#include "wifi_form.h"

/* Networks offered on the setup page: the strongest few that are usable. A
 * weaker or hidden network can still be typed in by name. */
#define SETUP_SCAN_LIST_MAX  10
#define SETUP_SCAN_MIN_RSSI  (-80) /* dBm; weaker links don't stay connected reliably */

typedef enum {
    SETUP_REASON_NONE,       /* entered by the user (or nothing configured) */
    SETUP_REASON_NO_CONNECT, /* saved network never reached */
    SETUP_REASON_AUTH,       /* saved network rejected the password */
} setup_reason_t;

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    int8_t rssi; /* dBm */
    bool secure;
} setup_network_t;
