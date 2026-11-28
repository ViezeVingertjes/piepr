#pragma once

/*
 * The values in a P1 telegram: meter readings, power, voltage, current and gas,
 * as integers in their smallest unit. Plain C, no ESP-IDF dependencies, no heap,
 * unit-tested (test/test_p1_parse).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One number from the telegram; `present` is false when the meter didn't send it,
 * or sent it in a form this parser doesn't accept. */
typedef struct {
    uint32_t value;
    bool present;
} p1_num_t;

/* A timestamp as the meter writes it: local time, two-digit year. */
typedef struct {
    uint8_t year; /* 0-99, for 2000-2099 */
    uint8_t month, day, hour, minute, second;
    bool summer; /* 'S': summer time, 'W': winter time */
    bool present;
} p1_time_t;

#define P1_PHASES 3

typedef struct {
    p1_time_t time;             /* 0-0:1.0.0 */
    p1_num_t version;           /* 1-3:0.2.8, e.g. 50 for DSMR 5.0; not sent in Belgium */
    p1_num_t tariff;            /* 0-0:96.14.0: usually 1 or 2 */
    p1_num_t delivered_wh[2];   /* 1-0:1.8.1, 1.8.2: per tariff */
    p1_num_t returned_wh[2];    /* 1-0:2.8.1, 2.8.2 */
    p1_num_t power_delivered_w; /* 1-0:1.7.0 */
    p1_num_t power_returned_w;  /* 1-0:2.7.0 */
    p1_num_t phase_delivered_w[P1_PHASES]; /* 1-0:21/41/61.7.0 */
    p1_num_t phase_returned_w[P1_PHASES];  /* 1-0:22/42/62.7.0 */
    p1_num_t voltage_dv[P1_PHASES];        /* 1-0:32/52/72.7.0, tenths of a volt */
    p1_num_t current_ma[P1_PHASES];        /* 1-0:31/51/71.7.0, milliamps */
    p1_num_t gas_l;                        /* 0-n:24.2.1 (Belgium: 24.2.3), litres */
    p1_time_t gas_time;                    /* when the gas meter took that reading */
} p1_values_t;

/* Fills `out` from one complete telegram (`len` bytes), as p1_framer_telegram()
 * returns it. Lines it doesn't know, or can't read, are skipped; the rest still
 * counts. Gas is taken from the first channel that says it is a gas meter
 * (device type 3), else from the first that doesn't say. Dates are checked
 * against the length of the month (29 February is always allowed). Returns the number of values
 * found. */
size_t p1_parse(const uint8_t *telegram, size_t len, p1_values_t *out);
