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
