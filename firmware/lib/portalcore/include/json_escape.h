#pragma once

/*
 * Escapes untrusted bytes (e.g. SSIDs) into the contents of a JSON string.
 * Plain C, no ESP-IDF dependencies, unit-tested and fuzzed on the host.
 */

#include <stdbool.h>
#include <stddef.h>

/* Output buffer size that always fits `len` input bytes: "\u00XX" per byte + NUL. */
#define JSON_ESCAPE_MAX(len) ((len) * 6 + 1)

/* Writes `in` (`len` bytes) to `out` as JSON string contents, without quotes,
 * NUL-terminated: '"' and '\' get a backslash, control bytes and DEL become
 * \u00xx, valid UTF-8 passes through and every other byte becomes U+FFFD.
 * Returns false (with `out` = "") when the result doesn't fit `out_size`. */
bool json_escape(const char *in, size_t len, char *out, size_t out_size);

/* True when all `len` bytes are valid, shortest-form UTF-8. */
bool utf8_valid(const char *in, size_t len);
