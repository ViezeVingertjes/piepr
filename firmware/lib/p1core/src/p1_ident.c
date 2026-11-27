#include "p1_ident.h"

#define FIRST_PRINTABLE 0x20
#define LAST_PRINTABLE  0x7E

size_t p1_ident(const uint8_t *telegram, size_t len, char *out, size_t out_size)
{
    if (out_size == 0) {
        return 0;
    }
    size_t n = 0;
    if (telegram != NULL && len > 0 && telegram[0] == '/') {
        for (size_t i = 1; i < len && n + 1 < out_size; i++) {
            uint8_t c = telegram[i];
            if (c == '\r' || c == '\n') {
                break;
            }
            out[n++] = c >= FIRST_PRINTABLE && c <= LAST_PRINTABLE ? (char)c : '?';
        }
    }
    out[n] = '\0';
    return n;
}
