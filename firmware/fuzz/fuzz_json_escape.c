/* libFuzzer harness: arbitrary bytes into json_escape. Run: fuzz/run.sh json_escape */
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json_escape.h"

/* Independent oracle: decodes one code point; returns its length or 0. */
static size_t decode_utf8(const uint8_t *s, size_t avail)
{
    size_t len = s[0] >= 0xF0 ? 4 : s[0] >= 0xE0 ? 3 : 2;
    uint32_t cp = s[0] & (0x7F >> len);
    if (s[0] < 0xC0 || avail < len) {
        return 0;
    }
    for (size_t i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            return 0;
        }
        cp = cp << 6 | (s[i] & 0x3F);
    }
    static const uint32_t min_for_len[] = { 0, 0, 0x80, 0x800, 0x10000 };
    bool valid = cp >= min_for_len[len] && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
    return valid ? len : 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t out_size = JSON_ESCAPE_MAX(size);
    char *out = malloc(out_size);
    assert(out != NULL);
    assert(json_escape((const char *)data, size, out, out_size));

    const uint8_t *o = (const uint8_t *)out;
    size_t n = strlen(out);
    for (size_t i = 0; i < n;) {
        if (o[i] == '\\') {
            assert(i + 1 < n);
            if (o[i + 1] == 'u') {
                assert(i + 5 < n);
                for (size_t k = 2; k < 6; k++) {
                    assert(isxdigit(o[i + k]));
                }
                i += 6;
            } else {
                assert(o[i + 1] == '"' || o[i + 1] == '\\');
                i += 2;
            }
        } else if (o[i] < 0x80) {
            assert(o[i] >= 0x20 && o[i] != 0x7F && o[i] != '"');
            i++;
        } else {
            size_t len = decode_utf8(&o[i], n - i);
            assert(len > 0);
            i += len;
        }
    }

    if (size > 0) {
        char small[1];
        assert(!json_escape((const char *)data, size, small, sizeof(small)) && small[0] == '\0');
    }
    free(out);
    return 0;
}
