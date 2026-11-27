#include "json_escape.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONTROL_END     0x20
#define DEL             0x7F
#define ASCII_END       0x80
#define UNICODE_ESCAPE  "\\u%04x"
#define UNICODE_ESC_LEN 6
#define REPLACEMENT     "\xEF\xBF\xBD" /* U+FFFD */

static bool is_continuation(uint8_t b)
{
    return (b & 0xC0) == 0x80;
}

/* Length of the valid, shortest-form UTF-8 sequence at `s` (no surrogates,
 * at most U+10FFFF), or 0 when the byte at `s` doesn't start one. */
static size_t utf8_sequence_len(const uint8_t *s, size_t avail)
{
    uint8_t lead = s[0];
    size_t len;
    uint8_t second_min = 0x80;
    uint8_t second_max = 0xBF;

    if (lead >= 0xC2 && lead <= 0xDF) {
        len = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        len = 3;
        if (lead == 0xE0) {
            second_min = 0xA0; /* overlong */
        } else if (lead == 0xED) {
            second_max = 0x9F; /* surrogates */
        }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        len = 4;
        if (lead == 0xF0) {
            second_min = 0x90; /* overlong */
        } else if (lead == 0xF4) {
            second_max = 0x8F; /* above U+10FFFF */
        }
    } else {
        return 0;
    }
    if (avail < len || s[1] < second_min || s[1] > second_max) {
        return 0;
    }
    for (size_t i = 2; i < len; i++) {
        if (!is_continuation(s[i])) {
            return 0;
        }
    }
    return len;
}

bool utf8_valid(const char *in, size_t len)
{
    const uint8_t *s = (const uint8_t *)in;
    for (size_t i = 0; i < len;) {
        size_t n = s[i] < ASCII_END ? 1 : utf8_sequence_len(&s[i], len - i);
        if (n == 0) {
            return false;
        }
        i += n;
    }
    return true;
}

bool json_escape(const char *in, size_t len, char *out, size_t out_size)
{
    if (out_size == 0) {
        return false;
    }
    const uint8_t *s = (const uint8_t *)in;
    size_t o = 0;

    for (size_t i = 0; i < len;) {
        char esc[UNICODE_ESC_LEN + 1];
        const char *piece = (const char *)&s[i];
        size_t piece_len = s[i] < ASCII_END ? 1 : utf8_sequence_len(&s[i], len - i);
        size_t consumed = piece_len;

        if (s[i] == '"' || s[i] == '\\') {
            piece = s[i] == '"' ? "\\\"" : "\\\\";
            piece_len = 2;
        } else if (s[i] < CONTROL_END || s[i] == DEL) {
            snprintf(esc, sizeof(esc), UNICODE_ESCAPE, s[i]);
            piece = esc;
            piece_len = UNICODE_ESC_LEN;
        } else if (piece_len == 0) {
            piece = REPLACEMENT;
            piece_len = sizeof(REPLACEMENT) - 1;
            consumed = 1;
        }

        if (o + piece_len >= out_size) {
            out[0] = '\0';
            return false;
        }
        memcpy(&out[o], piece, piece_len);
        o += piece_len;
        i += consumed;
    }
    out[o] = '\0';
    return true;
}
