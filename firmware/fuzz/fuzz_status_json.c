/* libFuzzer harness: any status into status_json. Run: fuzz/run.sh status_json */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "status_json.h"

typedef struct {
    const uint8_t *data;
    size_t size;
} input_t;

/* The next `n` bytes of the input, zeros once it runs out. */
static void take(input_t *in, void *out, size_t n)
{
    size_t have = n < in->size ? n : in->size;
    memset(out, 0, n);
    memcpy(out, in->data, have);
    in->data += have;
    in->size -= have;
}

static uint32_t take_u32(input_t *in)
{
    uint32_t v;
    take(in, &v, sizeof(v));
    return v;
}

static bool take_bool(input_t *in)
{
    uint8_t v;
    take(in, &v, 1);
    return v & 1;
}

static p1_time_t take_time(input_t *in)
{
    uint8_t f[6];
    take(in, f, sizeof(f));
    return (p1_time_t){ f[0], f[1], f[2], f[3], f[4], f[5], take_bool(in), take_bool(in) };
}

/* Every number in `v`, by name. */
static size_t all_nums(p1_values_t *v, p1_num_t **out)
{
    size_t n = 0;
    out[n++] = &v->version;
    out[n++] = &v->tariff;
    out[n++] = &v->power_delivered_w;
    out[n++] = &v->power_returned_w;
    out[n++] = &v->gas_l;
    for (int i = 0; i < 2; i++) {
        out[n++] = &v->delivered_wh[i];
        out[n++] = &v->returned_wh[i];
    }
    for (int i = 0; i < P1_PHASES; i++) {
        out[n++] = &v->phase_delivered_w[i];
        out[n++] = &v->phase_returned_w[i];
        out[n++] = &v->voltage_dv[i];
        out[n++] = &v->current_ma[i];
    }
    return n;
}
#define NUMS_MAX 32

/* Well-formed enough for a browser: strings closed, no raw control characters,
 * every brace and bracket closed in order. */
static void check_json(const char *json, size_t len)
{
    char open[8];
    size_t depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)json[i];
        assert(c >= 0x20 && c != 0x7F);
        if (in_string) {
            if (c == '\\') {
                assert(++i < len);
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            assert(depth < sizeof(open));
            open[depth++] = (char)c;
        } else if (c == '}' || c == ']') {
            assert(depth > 0 && open[--depth] == (c == '}' ? '{' : '['));
        }
    }
    assert(!in_string && depth == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    input_t in = { data, size };
    char firmware[STATUS_TEXT_MAX + 8], board[STATUS_TEXT_MAX + 8];
    take(&in, firmware, sizeof(firmware) - 1);
    take(&in, board, sizeof(board) - 1);
    firmware[sizeof(firmware) - 1] = board[sizeof(board) - 1] = '\0';

    status_t s = { .firmware = firmware, .board = board };
    s.uptime_s = take_u32(&in);
    s.has_rssi = take_bool(&in);
    s.rssi = (int)take_u32(&in);
    s.readers = take_u32(&in);
    s.readers_max = take_u32(&in);
    s.p1.counts = (p1_counts_t){ take_u32(&in), take_u32(&in), take_u32(&in), take_u32(&in) };
    s.p1.seen = take_bool(&in);
    s.p1.age_ms = take_u32(&in);
    s.p1.rejected = take_bool(&in);
    s.p1.rejected_age_ms = take_u32(&in);
    take(&in, s.p1.meter, P1_METER_NAME_MAX); /* may fill it, without a NUL to spare */
    p1_values_t *v = &s.p1.values;
    p1_num_t *nums[NUMS_MAX];
    size_t count = all_nums(v, nums);
    for (size_t i = 0; i < count; i++) {
        nums[i]->value = take_u32(&in);
        nums[i]->present = take_bool(&in);
    }
    v->time = take_time(&in);
    v->gas_time = take_time(&in);

    char json[STATUS_JSON_MAX];
    size_t len = status_json(&s, json, sizeof(json));
    assert(len > 0); /* STATUS_JSON_MAX fits any status */
    assert(len < sizeof(json) && json[len] == '\0' && strlen(json) == len);
    check_json(json, len);

    /* One byte short: nothing, and nothing written past the end. */
    char small[STATUS_JSON_MAX + 1];
    small[len] = 0x55;
    assert(status_json(&s, small, len) == 0 && small[0] == '\0' && small[len] == 0x55);
    return 0;
}
