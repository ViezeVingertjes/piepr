/* libFuzzer harness: arbitrary bytes into the P1 parser. Run: fuzz/run.sh p1_parse */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

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

/* Field by field: padding bytes may differ between two equal structs. */
static void check_same(const p1_values_t *a, const p1_values_t *b)
{
    const p1_num_t *na = &a->version, *nb = &b->version;
    size_t count = (size_t)(&a->gas_l - na) + 1; /* every p1_num_t, version .. gas_l */
    for (size_t i = 0; i < count; i++) {
        assert(na[i].present == nb[i].present && na[i].value == nb[i].value);
    }
    const p1_time_t *ta[] = { &a->time, &a->gas_time }, *tb[] = { &b->time, &b->gas_time };
    for (size_t i = 0; i < 2; i++) {
        assert(ta[i]->present == tb[i]->present && ta[i]->year == tb[i]->year &&
               ta[i]->month == tb[i]->month && ta[i]->day == tb[i]->day &&
               ta[i]->hour == tb[i]->hour && ta[i]->minute == tb[i]->minute &&
               ta[i]->second == tb[i]->second && ta[i]->summer == tb[i]->summer);
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
    check_same(&v, &again);
    return 0;
}
