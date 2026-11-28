#pragma once

/*
 * What the status page shows, as JSON: the device, its readers and the meter's
 * values. Plain C, no ESP-IDF dependencies, no heap; unit-tested and fuzzed.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "p1_status.h"

#define STATUS_TEXT_MAX 32 /* characters of `firmware` and of `board` that are used */

typedef struct {
    const char *firmware; /* version, e.g. "1.4.0" */
    const char *board;
    uint32_t uptime_s;
    bool has_rssi;                 /* connected to WiFi, with a signal strength */
    int rssi;                      /* dBm */
    uint32_t readers, readers_max; /* connected on the telegram port, of how many */
    p1_status_t p1;
} status_t;

/* Fits any status. */
#define STATUS_JSON_MAX 1536

/*
 * Writes `s` to `out`, NUL-terminated:
 *
 * {"firmware":"1.4.0","board":"…","uptime_s":86400,
 *  "wifi":{"rssi":-61},
 *  "readers":{"connected":2,"max":5},
 *  "telegrams":{"ok":86000,"crc_errors":2,"overflows":0,"uart_errors":0,
 *    "last_ok_ms":640,"last_rejected_ms":3600000},
 *  "meter":{"name":"ISK5\\2M550T-1013","dsmr_version":50,"time":"2026-11-28T15:33:42+01:00",
 *    "tariff":2,"delivered_t1_wh":…,"delivered_t2_wh":…,"returned_t1_wh":…,"returned_t2_wh":…,
 *    "power_delivered_w":412,"power_returned_w":0,
 *    "phases":[{"delivered_w":…,"returned_w":…,"voltage_dv":2314,"current_ma":1000},{…},{…}],
 *    "gas_l":2218934,"gas_time":"2026-11-28T15:30:00+01:00"}}
 *
 * Whole numbers in the smallest unit: Wh, W, tenths of a volt, mA, litres. Times
 * are the meter's own, with +01:00 for winter time and +02:00 for summer time.
 * The counts are since start; "last_ok_ms" is how long ago the last valid telegram
 * arrived, "last_rejected_ms" how long ago one failed its checksum or was too long.
 * A value that isn't known is left out: "rssi" without WiFi, "last_ok_ms" and
 * everything about the meter until a telegram arrived, "last_rejected_ms" while
 * none was rejected, and whatever the meter doesn't send. The objects themselves,
 * and three phases, are always there.
 * Returns the length, or 0 (with `out` = "") when it doesn't fit `out_size`.
 */
size_t status_json(const status_t *s, char *out, size_t out_size);
