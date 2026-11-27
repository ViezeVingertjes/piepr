/* libFuzzer harness: arbitrary bytes into the P1 framer. Run: fuzz/run.sh p1_framer */
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crc16.h"
#include "p1_framer.h"

/* Independent check of an emitted telegram: '/' ... '!' XXXX [\r] \n with a matching CRC. */
static void check_telegram(const uint8_t *t, size_t len)
{
    assert(len >= 7 && len <= P1_MAX_TELEGRAM);
    assert(t[0] == '/');
    assert(t[len - 1] == '\n');
    size_t bang = len - (t[len - 2] == '\r' ? 7 : 6);
    assert(t[bang] == '!');
    /* Decoded independently of the framer's own hex parser. */
    char digits[5] = { 0 };
    memcpy(digits, &t[bang + 1], 4);
    for (int i = 0; i < 4; i++) {
        assert(isxdigit((unsigned char)digits[i]));
    }
    uint16_t crc = (uint16_t)strtoul(digits, NULL, 16);
    assert(crc16_arc(t, bang + 1) == crc);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static p1_framer_t f; /* 4 KB: not on the stack */
    p1_framer_init(&f);
    for (size_t i = 0; i < size; i++) {
        if (p1_framer_push(&f, data[i]) == P1_FRAME_OK) {
            size_t len;
            const uint8_t *t = p1_framer_telegram(&f, &len);
            check_telegram(t, len);
        }
    }
    return 0;
}
