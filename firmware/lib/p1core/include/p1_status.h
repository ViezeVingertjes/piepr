#pragma once

/* What is known about the meter at one moment: how many telegrams arrived and
 * the values in the last valid one. Filled in by the reader (src/p1_reader.c). */

#include <stdbool.h>
#include <stdint.h>

#include "p1_parse.h"

#define P1_METER_NAME_MAX 32 /* characters of the meter's type name that are kept */

/* Counted since start. */
typedef struct {
    uint32_t ok;          /* valid telegrams */
    uint32_t crc_errors;  /* telegrams that failed their checksum */
    uint32_t overflows;   /* telegrams longer than a telegram can be */
    uint32_t uart_errors; /* times the serial line lost bytes or went silent mid-byte */
} p1_counts_t;

typedef struct {
    p1_counts_t counts;
    bool seen;                /* a valid telegram arrived since start */
    uint32_t age_ms;          /* how long ago the last one did */
    bool rejected;            /* a telegram was rejected since start: checksum, or too long */
    uint32_t rejected_age_ms; /* how long ago the last one was */
    char meter[P1_METER_NAME_MAX + 1]; /* the first line of the last valid telegram, e.g.
                                          "ISK5\2M550T-1013" */
    p1_values_t values;                /* and its values */
} p1_status_t;
