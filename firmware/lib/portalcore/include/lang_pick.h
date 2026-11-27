#pragma once

/*
 * Which language the setup pages are shown in, from the browser's
 * Accept-Language header. Plain C, no ESP-IDF dependencies, unit-tested.
 */

#include <stdbool.h>
#include <stddef.h>

/* True when `accept_language` (`len` bytes, e.g. "nl-NL,nl;q=0.9,en;q=0.8")
 * ranks Dutch above English; with equal weights the one listed first wins.
 * False when Dutch is not asked for, and for anything that can't be parsed:
 * English is the fallback. */
bool lang_prefers_dutch(const char *accept_language, size_t len);

typedef enum {
    LANG_UNSET,
    LANG_ENGLISH,
    LANG_DUTCH,
} lang_choice_t;

/* The language a URL query asks for with "lang=nl" or "lang=en" (`len` bytes,
 * without the '?', e.g. "lang=nl" or "a=1&lang=en"). LANG_UNSET when there is
 * no such parameter or it names another language; the first one counts. */
lang_choice_t lang_from_query(const char *query, size_t len);
