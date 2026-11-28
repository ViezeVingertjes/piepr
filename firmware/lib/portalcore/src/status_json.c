#include "status_json.h"

#include "json_writer.h"

static void put_u32(json_writer_t *w, const char *key, uint32_t value)
{
    json_key(w, key);
    json_put(w, "%lu", (unsigned long)value);
}

static void put_num(json_writer_t *w, const char *key, p1_num_t n)
{
    if (n.present) {
        put_u32(w, key, n.value);
    }
}

/* The meter's clock is local time in the Netherlands, Belgium and Luxembourg. */
static void put_time(json_writer_t *w, const char *key, const p1_time_t *t)
{
    if (t->present) {
        json_key(w, key);
        json_put(w, "\"20%02u-%02u-%02uT%02u:%02u:%02u+0%d:00\"", t->year, t->month, t->day,
                 t->hour, t->minute, t->second, t->summer ? 2 : 1);
    }
}

static void put_meter(json_writer_t *w, const p1_status_t *p1)
{
    static const p1_values_t NOTHING;
    const p1_values_t *v = p1->seen ? &p1->values : &NOTHING;

    json_key(w, "meter");
    json_put(w, "{");
    if (p1->seen) {
        json_key(w, "name");
        json_put_string(w, p1->meter, P1_METER_NAME_MAX);
    }
    put_num(w, "dsmr_version", v->version);
    put_time(w, "time", &v->time);
    put_num(w, "tariff", v->tariff);
    put_num(w, "delivered_t1_wh", v->delivered_wh[0]);
    put_num(w, "delivered_t2_wh", v->delivered_wh[1]);
    put_num(w, "returned_t1_wh", v->returned_wh[0]);
    put_num(w, "returned_t2_wh", v->returned_wh[1]);
    put_num(w, "power_delivered_w", v->power_delivered_w);
    put_num(w, "power_returned_w", v->power_returned_w);
    json_key(w, "phases");
    for (int i = 0; i < P1_PHASES; i++) {
        json_put(w, i == 0 ? "[{" : ",{");
        put_num(w, "delivered_w", v->phase_delivered_w[i]);
        put_num(w, "returned_w", v->phase_returned_w[i]);
        put_num(w, "voltage_dv", v->voltage_dv[i]);
        put_num(w, "current_ma", v->current_ma[i]);
        json_put(w, "}");
    }
    json_put(w, "]");
    put_num(w, "gas_l", v->gas_l);
    put_time(w, "gas_time", &v->gas_time);
    json_put(w, "}");
}

size_t status_json(const status_t *s, char *out, size_t out_size)
{
    const p1_status_t *p1 = &s->p1;
    json_writer_t w;
    json_writer_init(&w, out, out_size);

    json_put(&w, "{");
    json_key(&w, "firmware");
    json_put_string(&w, s->firmware, STATUS_TEXT_MAX);
    json_key(&w, "board");
    json_put_string(&w, s->board, STATUS_TEXT_MAX);
    put_u32(&w, "uptime_s", s->uptime_s);

    json_key(&w, "wifi");
    json_put(&w, "{");
    if (s->has_rssi) {
        json_key(&w, "rssi");
        json_put(&w, "%d", s->rssi);
    }
    json_put(&w, "}");

    json_key(&w, "readers");
    json_put(&w, "{");
    put_u32(&w, "connected", s->readers);
    put_u32(&w, "max", s->readers_max);
    json_put(&w, "}");

    json_key(&w, "telegrams");
    json_put(&w, "{");
    put_u32(&w, "ok", p1->counts.ok);
    put_u32(&w, "crc_errors", p1->counts.crc_errors);
    put_u32(&w, "overflows", p1->counts.overflows);
    put_u32(&w, "uart_errors", p1->counts.uart_errors);
    if (p1->seen) {
        put_u32(&w, "last_ok_ms", p1->age_ms);
    }
    if (p1->rejected) {
        put_u32(&w, "last_rejected_ms", p1->rejected_age_ms);
    }
    json_put(&w, "}");

    put_meter(&w, p1);
    json_put(&w, "}");
    return json_writer_finish(&w);
}
