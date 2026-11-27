#pragma once

/*
 * What the setup page shows: the nearby networks from the last scan and why
 * the device entered setup. Plain C, no ESP-IDF dependencies, unit-tested.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "json_escape.h"
#include "setup_types.h"

/* Adds one scan result to `list` (`*count` entries, at most SETUP_SCAN_LIST_MAX),
 * kept sorted strongest first. Networks weaker than SETUP_SCAN_MIN_RSSI, and
 * hidden (empty), over-long and non-UTF-8 SSIDs (which the page can't show or
 * post back unchanged) are ignored;
 * a name already listed keeps only its strongest entry; on a full list the
 * weakest entry makes way, or the new one is dropped if it is the weakest. */
void setup_networks_add(setup_network_t *list, size_t *count, const char *ssid, size_t ssid_len,
                        int8_t rssi, bool secure);

/* One network in a survey: its signal summed over several scans, so a momentary
 * dip doesn't decide the order. */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    int16_t rssi_sum; /* of the strongest reading per scan */
    uint8_t scans;    /* scans that saw it */
    uint8_t last_scan;
    int8_t scan_rssi; /* strongest reading in `last_scan` */
    bool secure;
} setup_survey_entry_t;

/* Adds one reading from scan number `scan` (0, 1, ...) to `survey` (`*count`
 * entries, at most `max`). A name seen twice in one scan (several access points)
 * counts once, with its strongest reading; it is secure if any of them is. Invalid
 * names are ignored; when the survey is full, a new name replaces the weakest one
 * if its reading is stronger than that one's average. */
void setup_survey_add(setup_survey_entry_t *survey, size_t *count, size_t max, uint8_t scan,
                      const char *ssid, size_t ssid_len, int8_t rssi, bool secure);

/* Fills `list` like setup_networks_add, using each network's average signal. */
void setup_survey_result(const setup_survey_entry_t *survey, size_t count, setup_network_t *list,
                         size_t *list_count);

#define SETUP_STATE_JSON_FIXED                                                                     \
    "{\"reason\":\"no_connect\",\"reason_ssid\":\"\",\"scanning\":false,\"networks\":[]}"
#define SETUP_STATE_JSON_NETWORK "{\"ssid\":\"\",\"rssi\":-128,\"secure\":false},"
#define SETUP_STATE_JSON_MAX                                                                       \
    (sizeof(SETUP_STATE_JSON_FIXED) + JSON_ESCAPE_MAX(WIFI_SSID_MAX_LEN) +                         \
     SETUP_SCAN_LIST_MAX *                                                                         \
         (sizeof(SETUP_STATE_JSON_NETWORK) + JSON_ESCAPE_MAX(WIFI_SSID_MAX_LEN)))

/* "none", "no_connect" or "auth" (also the JSON value). */
const char *setup_reason_name(setup_reason_t reason);

/* Writes {"reason":"none|no_connect|auth","reason_ssid":"…","scanning":false,"networks":[{"ssid":
 * "…","rssi":-60,"secure":true},…]} to `out`, NUL-terminated. `reason_ssid`
 * may be NULL. Returns the length, or 0 (with `out` = "") when it doesn't fit. */
size_t setup_state_json(setup_reason_t reason, const char *reason_ssid, const setup_network_t *nets,
                        size_t count, bool scanning, char *out, size_t out_size);
