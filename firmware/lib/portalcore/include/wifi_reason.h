#pragma once

/*
 * Why the WiFi connection ended, in words, for the log. Plain C, no ESP-IDF
 * dependencies, unit-tested. The numbers are ESP-IDF's wifi_err_reason_t;
 * src/wifi.c checks at build time that they still match.
 */

#define WIFI_REASON_TEXT_AUTH_EXPIRE       2
#define WIFI_REASON_TEXT_ASSOC_LEAVE       8
#define WIFI_REASON_TEXT_4WAY_TIMEOUT      15
#define WIFI_REASON_TEXT_BEACON_TIMEOUT    200
#define WIFI_REASON_TEXT_NO_AP_FOUND       201
#define WIFI_REASON_TEXT_AUTH_FAIL         202
#define WIFI_REASON_TEXT_ASSOC_FAIL        203
#define WIFI_REASON_TEXT_HANDSHAKE_TIMEOUT 204
#define WIFI_REASON_TEXT_CONNECTION_FAIL   205

/* A few words for the reasons a user can act on, e.g. "network not found";
 * NULL for the others, which are logged by number only. */
const char *wifi_reason_text(unsigned reason);
