#pragma once

/*
 * DSMR P1 telegram framer: collects bytes from '/' up to "!XXXX\r\n", checks
 * the CRC16 and reports complete, valid telegrams. Plain C, no ESP-IDF
 * dependencies, unit-tested on the host (test/test_p1_framer).
 */

#include <stddef.h>
#include <stdint.h>

#define P1_MAX_TELEGRAM 4096 /* operator text 0-0:96.13.0 alone can be 2 KB */

typedef enum {
    P1_FRAME_NONE,      /* byte consumed, no complete telegram yet */
    P1_FRAME_OK,        /* complete telegram with a valid CRC is available */
    P1_FRAME_CRC_ERROR, /* telegram ended but CRC missing/invalid/mismatched */
    P1_FRAME_OVERFLOW,  /* telegram exceeded P1_MAX_TELEGRAM, discarded */
} p1_frame_result_t;

/* Framer state. Treat as opaque: use the functions below. */
typedef struct {
    uint8_t buf[P1_MAX_TELEGRAM];
    size_t len;
    size_t crc_end;     /* number of bytes covered by the CRC ('/' .. '!') */
    uint16_t crc_value; /* CRC parsed from the telegram */
    uint8_t state;      /* framer_state_t, see p1_framer.c */
    uint8_t crc_digits; /* CRC hex digits received so far */
} p1_framer_t;

/* Resets the framer; it then waits for the next '/'. */
void p1_framer_init(p1_framer_t *f);

/* Feeds one byte from the P1 line. Never blocks, never allocates. */
p1_frame_result_t p1_framer_push(p1_framer_t *f, uint8_t byte);

/* The complete telegram ('/' .. "!XXXX\r\n"). Valid only directly after
 * p1_framer_push() returned P1_FRAME_OK, until the next push. */
const uint8_t *p1_framer_telegram(const p1_framer_t *f, size_t *len);
