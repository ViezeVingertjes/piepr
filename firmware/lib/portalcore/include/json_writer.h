#pragma once

/*
 * Writes JSON into a fixed buffer, piece by piece; once something doesn't fit,
 * the rest is skipped and the result is empty. Plain C, no ESP-IDF
 * dependencies, no heap; tested through setup_state and status_json.
 */

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *buf;
    size_t size;
    size_t len;
    bool overflow;
} json_writer_t;

void json_writer_init(json_writer_t *w, char *out, size_t out_size);

/* Adds text as it is, printf style. */
void json_put(json_writer_t *w, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* Adds `s` as a JSON string, quoted and escaped (see json_escape): at most
 * `max_len` bytes of it, fewer when a NUL comes first. */
void json_put_string(json_writer_t *w, const char *s, size_t max_len);

/* Adds "key": for the next value, with a comma first unless it is the first
 * thing in its object. */
void json_key(json_writer_t *w, const char *key);

/* The length written; the text is NUL-terminated. 0, with the buffer set to "",
 * when it didn't fit. */
size_t json_writer_finish(json_writer_t *w);
