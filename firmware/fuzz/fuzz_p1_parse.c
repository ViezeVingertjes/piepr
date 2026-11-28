/* libFuzzer harness: arbitrary bytes into the P1 parser. Run: fuzz/run.sh p1_parse */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "p1_parse.h"

static void check_time(const p1_time_t *t)
{
    if (t->present) {
        assert(t->year <= 99);
        assert(t->month >= 1 && t->month <= 12);
        assert(t->day >= 1 && t->day <= 31);
        assert(t->hour <= 23 && t->minute <= 59 && t->second <= 59);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    p1_values_t v, again;
    size_t found = p1_parse(data, size, &v);
    check_time(&v.time);
    check_time(&v.gas_time);
    assert(v.gas_l.present == v.gas_time.present);
    /* The same input always gives the same values. */
    assert(p1_parse(data, size, &again) == found);
    assert(memcmp(&v, &again, sizeof(v)) == 0);
    return 0;
}
