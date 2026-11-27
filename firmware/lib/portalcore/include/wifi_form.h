#pragma once

/*
 * Decodes and validates the WiFi setup form (application/x-www-form-urlencoded,
 * keys "ssid" and "pass"). Plain C, no ESP-IDF dependencies, unit-tested and
 * fuzzed on the host.
 */

#include <stddef.h>

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MIN_LEN 8
#define WIFI_PASS_MAX_LEN 63

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1]; /* NUL-terminated, 1..32 bytes */
    char pass[WIFI_PASS_MAX_LEN + 1]; /* NUL-terminated, empty (open network) or 8..63 bytes */
} wifi_credentials_t;

typedef enum {
    WIFI_FORM_OK,
    WIFI_FORM_MALFORMED, /* bad escape, NUL byte, missing or duplicate key */
    WIFI_FORM_BAD_SSID,  /* SSID not 1..32 bytes */
    WIFI_FORM_BAD_PASS,  /* password not empty or 8..63 bytes */
} wifi_form_result_t;

/* Parses `body` (not NUL-terminated, `len` bytes). `out` is only written on
 * WIFI_FORM_OK. Unknown keys are ignored. */
wifi_form_result_t wifi_form_parse(const char *body, size_t len, wifi_credentials_t *out);
