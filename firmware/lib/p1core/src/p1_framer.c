#include "p1_framer.h"

#include <stdbool.h>

#include "crc16.h"

/*
 * Robustness rules:
 *  - '/' in ANY state starts a new telegram, so the framer always resyncs.
 *  - The buffer is fixed-size; exceeding it ends the frame (OVERFLOW).
 *  - After "!XXXX" only "\r\n" (or "\n") is accepted; anything else ends the
 *    frame as CRC_ERROR. There is no state without an exit.
 */
typedef enum {
    STATE_WAIT_START = 0, /* ignore everything until '/' */
    STATE_BODY,           /* collecting data lines until '!' */
    STATE_CRC,            /* collecting CRC_DIGITS hex digits */
    STATE_EOL,            /* expecting an optional '\r', then '\n' */
} framer_state_t;

#define CRC_DIGITS 4

static int hex_value(uint8_t c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

void p1_framer_init(p1_framer_t *f)
{
    f->len = 0;
    f->crc_end = 0;
    f->crc_value = 0;
    f->state = STATE_WAIT_START;
    f->crc_digits = 0;
}

static bool append(p1_framer_t *f, uint8_t byte)
{
    if (f->len >= P1_MAX_TELEGRAM) {
        return false;
    }
    f->buf[f->len++] = byte;
    return true;
}

static p1_frame_result_t fail(p1_framer_t *f, p1_frame_result_t why)
{
    f->state = STATE_WAIT_START;
    f->len = 0;
    return why;
}

p1_frame_result_t p1_framer_push(p1_framer_t *f, uint8_t byte)
{
    if (byte == '/') {
        f->len = 0;
        f->buf[f->len++] = byte;
        f->state = STATE_BODY;
        return P1_FRAME_NONE;
    }
    if (f->state == STATE_WAIT_START) {
        return P1_FRAME_NONE;
    }
    if (!append(f, byte)) {
        return fail(f, P1_FRAME_OVERFLOW);
    }

    switch (f->state) {
    case STATE_BODY:
        if (byte == '!') {
            f->crc_end = f->len;
            f->crc_digits = 0;
            f->crc_value = 0;
            f->state = STATE_CRC;
        }
        return P1_FRAME_NONE;

    case STATE_CRC: {
        int v = hex_value(byte);
        if (v < 0) {
            return fail(f, P1_FRAME_CRC_ERROR);
        }
        f->crc_value = (uint16_t)((f->crc_value << 4) | (uint16_t)v);
        if (++f->crc_digits == CRC_DIGITS) {
            f->state = STATE_EOL;
        }
        return P1_FRAME_NONE;
    }

    case STATE_EOL:
        /* At most one '\r' (the byte before it is then the last CRC digit). */
        if (byte == '\r' && f->buf[f->len - 2] != '\r') {
            return P1_FRAME_NONE;
        }
        if (byte != '\n') {
            return fail(f, P1_FRAME_CRC_ERROR);
        }
        if (crc16_arc(f->buf, f->crc_end) != f->crc_value) {
            return fail(f, P1_FRAME_CRC_ERROR);
        }
        f->state = STATE_WAIT_START;
        return P1_FRAME_OK; /* buf/len stay valid until the next push */

    default:
        return fail(f, P1_FRAME_NONE);
    }
}

const uint8_t *p1_framer_telegram(const p1_framer_t *f, size_t *len)
{
    *len = f->len;
    return f->buf;
}
