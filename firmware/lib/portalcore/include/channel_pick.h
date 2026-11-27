#pragma once

/*
 * Picks the least busy 2.4 GHz channel for the setup hotspot. Candidates are
 * CHANNEL_PICK_FIRST..CHANNEL_PICK_LAST: the band edges (1 and 12/13) are
 * excluded (1 is the most crowded; 12/13 are not allowed for a hotspot in
 * ESP-IDF's default world-safe regulatory setting). Plain C, unit-tested.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t channel; /* primary channel 1..14 */
    int8_t rssi;     /* dBm */
} channel_pick_ap_t;

#define CHANNEL_PICK_FIRST     2
#define CHANNEL_PICK_LAST      11
#define CHANNEL_PICK_DEFAULT   6
#define CHANNEL_PICK_VALID(ch) ((ch) >= CHANNEL_PICK_FIRST && (ch) <= CHANNEL_PICK_LAST)

/* Returns the candidate with the lowest load, where every AP within +/-4
 * channels (20 MHz overlap) adds (rssi + 100), clamped >= 0. Ties go to the
 * lower channel; no APs at all -> CHANNEL_PICK_DEFAULT. */
uint8_t channel_pick_quietest(const channel_pick_ap_t *aps, size_t count);
