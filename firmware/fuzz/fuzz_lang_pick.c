/* libFuzzer harness: arbitrary Accept-Language headers into lang_prefers_dutch,
 * and the same bytes as a URL query into lang_from_query.
 * Run: fuzz/run.sh lang_pick */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lang_pick.h"

static bool mentions_dutch(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i + 1 < size; i++) {
        if ((data[i] | 0x20) == 'n' && (data[i + 1] | 0x20) == 'l') {
            return true;
        }
    }
    return false;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* An exact-size copy, so reading past the end is caught. */
    char *header = malloc(size ? size : 1);
    assert(header != NULL);
    memcpy(header, data, size);

    bool dutch = lang_prefers_dutch(header, size);
    assert(!dutch || mentions_dutch(data, size));
    /* A list that ends in a comma means the same. */
    assert(size == 0 || data[size - 1] != ',' || dutch == lang_prefers_dutch(header, size - 1));

    lang_choice_t asked = lang_from_query(header, size);
    assert(asked == LANG_UNSET || size >= sizeof("lang=nl") - 1);

    free(header);
    return 0;
}
