#pragma once

/*
 * The identification of a P1 telegram: its first line, which names the meter
 * type. Plain C, no ESP-IDF dependencies, unit-tested.
 */

#include <stddef.h>
#include <stdint.h>

/* Copies the first line of `telegram` (`len` bytes) without its leading '/',
 * e.g. "ISK5\2M550E-1012", into `out` as a C string: at most `out_size` - 1
 * characters, with '?' for anything that isn't printable ASCII. Returns the
 * number of characters written; 0 when the telegram doesn't start with '/'. */
size_t p1_ident(const uint8_t *telegram, size_t len, char *out, size_t out_size);
