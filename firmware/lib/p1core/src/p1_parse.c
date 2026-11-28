#include "p1_parse.h"

#include <string.h>

/* A line is "OBIS(value)(value)...": the OBIS code, then one or more groups. */
typedef struct {
    const uint8_t *s;
    size_t len;
} span_t;

/* The plain numbers: OBIS code, where the value goes, its unit (NULL: none) and
 * how many decimals the stored value has (kWh with 3 decimals is Wh). */
typedef struct {
    const char *obis;
    size_t offset;
    const char *unit;
    uint8_t decimals;
} field_t;

#define FIELD(obis, member, unit, decimals) { obis, offsetof(p1_values_t, member), unit, decimals }

static const field_t FIELDS[] = {
    FIELD("1-3:0.2.8", version, NULL, 0),
    FIELD("0-0:96.14.0", tariff, NULL, 0),
    FIELD("1-0:1.8.1", delivered_wh[0], "kWh", 3),
    FIELD("1-0:1.8.2", delivered_wh[1], "kWh", 3),
    FIELD("1-0:2.8.1", returned_wh[0], "kWh", 3),
    FIELD("1-0:2.8.2", returned_wh[1], "kWh", 3),
    FIELD("1-0:1.7.0", power_delivered_w, "kW", 3),
    FIELD("1-0:2.7.0", power_returned_w, "kW", 3),
    FIELD("1-0:21.7.0", phase_delivered_w[0], "kW", 3),
    FIELD("1-0:41.7.0", phase_delivered_w[1], "kW", 3),
    FIELD("1-0:61.7.0", phase_delivered_w[2], "kW", 3),
    FIELD("1-0:22.7.0", phase_returned_w[0], "kW", 3),
    FIELD("1-0:42.7.0", phase_returned_w[1], "kW", 3),
    FIELD("1-0:62.7.0", phase_returned_w[2], "kW", 3),
    FIELD("1-0:32.7.0", voltage_dv[0], "V", 1),
    FIELD("1-0:52.7.0", voltage_dv[1], "V", 1),
    FIELD("1-0:72.7.0", voltage_dv[2], "V", 1),
    FIELD("1-0:31.7.0", current_ma[0], "A", 3),
    FIELD("1-0:51.7.0", current_ma[1], "A", 3),
    FIELD("1-0:71.7.0", current_ma[2], "A", 3),
};

#define FIELD_COUNT  (sizeof(FIELDS) / sizeof(FIELDS[0]))
#define GAS_CHANNELS 4  /* M-Bus channels 0-1 .. 0-4 */
#define DEVICE_GAS   3  /* 0-n:24.1.0: device type of a gas meter */
#define TIME_DIGITS  12 /* YYMMDDhhmmss, then S or W */

static bool is_digit(uint8_t c)
{
    return c >= '0' && c <= '9';
}

static uint8_t lower(uint8_t c)
{
    return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c;
}

static bool span_is(span_t a, const char *text)
{
    return a.len == strlen(text) && memcmp(a.s, text, a.len) == 0;
}

static bool span_is_ignoring_case(span_t a, const char *text)
{
    if (a.len != strlen(text)) {
        return false;
    }
    for (size_t i = 0; i < a.len; i++) {
        if (lower(a.s[i]) != lower((uint8_t)text[i])) {
            return false;
        }
    }
    return true;
}

/* The `index`-th "(...)" group in `groups` (the line from its first '('),
 * without the brackets. */
static bool group(span_t groups, size_t index, span_t *out)
{
    size_t i = 0;
    for (size_t n = 0; i < groups.len && groups.s[i] == '('; n++) {
        size_t start = ++i;
        while (i < groups.len && groups.s[i] != ')') {
            i++;
        }
        if (i == groups.len) {
            return false; /* no closing bracket */
        }
        if (n == index) {
            *out = (span_t){ groups.s + start, i - start };
            return true;
        }
        i++;
    }
    return false;
}

/* "001234.567*kWh" -> 1234567 with 3 decimals. Fewer decimals than `decimals`
 * are filled up, more are cut off. The unit has to match (in any case). */
static bool parse_number(span_t g, const char *unit, uint8_t decimals, uint32_t *out)
{
    uint64_t v = 0;
    size_t i = 0;
    while (i < g.len && is_digit(g.s[i])) {
        v = v * 10 + (uint64_t)(g.s[i++] - '0');
        if (v > UINT32_MAX) {
            return false;
        }
    }
    if (i == 0) {
        return false;
    }
    unsigned frac = 0;
    if (i < g.len && g.s[i] == '.') {
        size_t start = ++i;
        for (; i < g.len && is_digit(g.s[i]); i++) {
            if (frac < decimals) {
                v = v * 10 + (uint64_t)(g.s[i] - '0');
                frac++;
            }
        }
        if (i == start) {
            return false;
        }
    }
    for (; frac < decimals; frac++) {
        v *= 10;
    }
    if (v > UINT32_MAX) {
        return false;
    }
    if (unit == NULL) {
        if (i != g.len) {
            return false;
        }
    } else if (i == g.len || g.s[i] != '*' ||
               !span_is_ignoring_case((span_t){ g.s + i + 1, g.len - i - 1 }, unit)) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static unsigned two_digits(const uint8_t *s)
{
    return (unsigned)(s[0] - '0') * 10 + (unsigned)(s[1] - '0');
}

/* "261128150934W" -> 2026-11-28 15:09:34, winter time. */
static bool parse_time(span_t g, p1_time_t *out)
{
    if (g.len != TIME_DIGITS + 1 || (g.s[TIME_DIGITS] != 'S' && g.s[TIME_DIGITS] != 'W')) {
        return false;
    }
    for (size_t i = 0; i < TIME_DIGITS; i++) {
        if (!is_digit(g.s[i])) {
            return false;
        }
    }
    p1_time_t t = {
        .year = (uint8_t)two_digits(g.s),
        .month = (uint8_t)two_digits(g.s + 2),
        .day = (uint8_t)two_digits(g.s + 4),
        .hour = (uint8_t)two_digits(g.s + 6),
        .minute = (uint8_t)two_digits(g.s + 8),
        .second = (uint8_t)two_digits(g.s + 10),
        .summer = g.s[TIME_DIGITS] == 'S',
        .present = true,
    };
    if (t.month < 1 || t.month > 12 || t.day < 1 || t.day > 31 || t.hour > 23 || t.minute > 59 ||
        t.second > 59) {
        return false;
    }
    *out = t;
    return true;
}

/* "0-n:<tail>" with channel n 1..GAS_CHANNELS: returns n and sets `tail`
 * (":24.2.1"), or returns 0. */
static unsigned mbus_channel(span_t obis, span_t *tail)
{
    if (obis.len < 4 || obis.s[0] != '0' || obis.s[1] != '-' || obis.s[2] < '1' ||
        obis.s[2] > '0' + GAS_CHANNELS) {
        return 0;
    }
    *tail = (span_t){ obis.s + 3, obis.len - 3 };
    return (unsigned)(obis.s[2] - '0');
}

static p1_num_t *field_num(const p1_values_t *v, const field_t *f)
{
    return (p1_num_t *)((const uint8_t *)v + f->offset);
}

typedef struct {
    p1_values_t *out;
    uint8_t device_type[GAS_CHANNELS + 1]; /* per channel; 0: not said (yet) */
} parser_t;

static void parse_gas(parser_t *p, span_t groups, unsigned channel)
{
    uint8_t type = p->device_type[channel];
    span_t when, reading;
    p1_time_t t;
    uint32_t litres;
    if (p->out->gas_l.present || (type != 0 && type != DEVICE_GAS) || !group(groups, 0, &when) ||
        !group(groups, 1, &reading) || !parse_time(when, &t) ||
        !parse_number(reading, "m3", 3, &litres)) {
        return;
    }
    p->out->gas_l = (p1_num_t){ litres, true };
    p->out->gas_time = t;
}

static void parse_line(parser_t *p, span_t line)
{
    size_t paren = 0;
    while (paren < line.len && line.s[paren] != '(') {
        paren++;
    }
    if (paren == line.len) {
        return; /* the meter type, an empty line or the end: no value */
    }
    span_t obis = { line.s, paren };
    span_t groups = { line.s + paren, line.len - paren };
    span_t g, tail;

    if (span_is(obis, "0-0:1.0.0")) {
        if (group(groups, 0, &g)) {
            parse_time(g, &p->out->time);
        }
        return;
    }
    unsigned channel = mbus_channel(obis, &tail);
    if (channel != 0) {
        uint32_t type;
        if (span_is(tail, ":24.1.0")) {
            if (group(groups, 0, &g) && parse_number(g, NULL, 0, &type) && type <= UINT8_MAX) {
                p->device_type[channel] = (uint8_t)type;
            }
        } else if (span_is(tail, ":24.2.1") || span_is(tail, ":24.2.3")) {
            parse_gas(p, groups, channel);
        }
        return;
    }
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const field_t *f = &FIELDS[i];
        if (span_is(obis, f->obis)) {
            uint32_t v;
            if (group(groups, 0, &g) && parse_number(g, f->unit, f->decimals, &v)) {
                *field_num(p->out, f) = (p1_num_t){ v, true };
            }
            return;
        }
    }
}

static size_t count_found(const p1_values_t *v)
{
    size_t n = (size_t)v->time.present + (size_t)v->gas_l.present;
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        n += field_num(v, &FIELDS[i])->present;
    }
    return n;
}

/* Lines end in "\r\n" (or "\n"); a trailing '\r' is never looked at, because
 * every value ends at its ')'. */
size_t p1_parse(const uint8_t *telegram, size_t len, p1_values_t *out)
{
    memset(out, 0, sizeof(*out));
    if (telegram == NULL) {
        return 0;
    }
    parser_t p = { .out = out };
    size_t start = 0;
    while (start < len) {
        size_t end = start;
        while (end < len && telegram[end] != '\n') {
            end++;
        }
        parse_line(&p, (span_t){ telegram + start, end - start });
        start = end + 1;
    }
    return count_found(out);
}
