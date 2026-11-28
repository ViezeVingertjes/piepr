#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "status_json.h"

void setUp(void) {}
void tearDown(void) {}

static char json[STATUS_JSON_MAX];

static p1_num_t num(uint32_t value)
{
    return (p1_num_t){ .value = value, .present = true };
}

/* A device that never saw a telegram and has no WiFi signal to report. */
static status_t fresh(void)
{
    status_t s = {
        .firmware = "1.4.0", .board = "ESP32-C3 SuperMini", .uptime_s = 42, .readers_max = 5
    };
    return s;
}

/* A three-phase DSMR 5 meter with gas, read by two readers. */
static status_t full(void)
{
    status_t s = fresh();
    s.uptime_s = 86400;
    s.has_rssi = true;
    s.rssi = -61;
    s.readers = 2;
    s.p1 = (p1_status_t){ .counts = { .ok = 86000, .crc_errors = 2, .uart_errors = 1 },
                          .seen = true,
                          .age_ms = 640,
                          .rejected = true,
                          .rejected_age_ms = 3600000 };
    strcpy(s.p1.meter, "ISK5\\2M550T-1013");
    p1_values_t *v = &s.p1.values;
    v->time = (p1_time_t){ 26, 11, 28, 15, 33, 42, false, true };
    v->version = num(50);
    v->tariff = num(2);
    v->delivered_wh[0] = num(4821377);
    v->delivered_wh[1] = num(3907142);
    v->returned_wh[0] = num(0);
    v->returned_wh[1] = num(12);
    v->power_delivered_w = num(412);
    v->power_returned_w = num(0);
    const uint32_t watts[] = { 301, 68, 43 }, volts[] = { 2314, 2298, 2331 };
    for (int i = 0; i < P1_PHASES; i++) {
        v->phase_delivered_w[i] = num(watts[i]);
        v->phase_returned_w[i] = num(0);
        v->voltage_dv[i] = num(volts[i]);
        v->current_ma[i] = num(i == 0 ? 1000 : 0);
    }
    v->gas_l = num(2218934);
    v->gas_time = (p1_time_t){ 26, 11, 28, 15, 30, 0, false, true };
    return s;
}

void test_full_status(void)
{
    status_t s = full();
    size_t n = status_json(&s, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING(
        "{\"firmware\":\"1.4.0\",\"board\":\"ESP32-C3 SuperMini\",\"uptime_s\":86400,"
        "\"wifi\":{\"rssi\":-61},"
        "\"readers\":{\"connected\":2,\"max\":5},"
        "\"telegrams\":{\"ok\":86000,\"crc_errors\":2,\"overflows\":0,\"uart_errors\":1,"
        "\"last_ok_ms\":640,\"last_rejected_ms\":3600000},"
        "\"meter\":{\"name\":\"ISK5\\\\2M550T-1013\",\"dsmr_version\":50,"
        "\"time\":\"2026-11-28T15:33:42+01:00\",\"tariff\":2,"
        "\"delivered_t1_wh\":4821377,\"delivered_t2_wh\":3907142,"
        "\"returned_t1_wh\":0,\"returned_t2_wh\":12,"
        "\"power_delivered_w\":412,\"power_returned_w\":0,"
        "\"phases\":["
        "{\"delivered_w\":301,\"returned_w\":0,\"voltage_dv\":2314,\"current_ma\":1000},"
        "{\"delivered_w\":68,\"returned_w\":0,\"voltage_dv\":2298,\"current_ma\":0},"
        "{\"delivered_w\":43,\"returned_w\":0,\"voltage_dv\":2331,\"current_ma\":0}],"
        "\"gas_l\":2218934,\"gas_time\":\"2026-11-28T15:30:00+01:00\"}}",
        json);
    TEST_ASSERT_EQUAL_size_t(strlen(json), n);
}

void test_nothing_seen_yet(void)
{
    status_t s = fresh();
    s.p1.counts.crc_errors = 7;
    s.p1.rejected = true;
    s.p1.rejected_age_ms = 500;
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING(
        "{\"firmware\":\"1.4.0\",\"board\":\"ESP32-C3 SuperMini\",\"uptime_s\":42,"
        "\"wifi\":{},"
        "\"readers\":{\"connected\":0,\"max\":5},"
        "\"telegrams\":{\"ok\":0,\"crc_errors\":7,\"overflows\":0,\"uart_errors\":0,"
        "\"last_rejected_ms\":500},"
        "\"meter\":{\"phases\":[{},{},{}]}}",
        json);
}

/* Until a telegram was seen, whatever the values hold is not shown. */
void test_values_are_ignored_until_a_telegram_was_seen(void)
{
    status_t s = full();
    s.p1.seen = false;
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"meter\":{\"phases\":[{},{},{}]}}"));
    TEST_ASSERT_NULL(strstr(json, "last_ok_ms"));
}

void test_no_rejected_telegram_no_time_for_it(void)
{
    status_t s = full();
    s.p1.rejected = false;
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"last_ok_ms\":640},"));
}

void test_a_value_the_meter_does_not_send_is_left_out(void)
{
    status_t s = full();
    p1_values_t *v = &s.p1.values;
    v->version.present = false; /* a Belgian meter: no version, one phase, no gas */
    v->gas_l.present = false;
    v->gas_time.present = false;
    v->returned_wh[0].present = false;
    for (int i = 0; i < P1_PHASES; i++) {
        v->phase_delivered_w[i].present = false;
        v->phase_returned_w[i].present = false;
        v->voltage_dv[i].present = i == 0;
        v->current_ma[i].present = i == 0;
    }
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(
        json, "\"meter\":{\"name\":\"ISK5\\\\2M550T-1013\",\"time\":\"2026-11-28T15:33:42+01:00\","
              "\"tariff\":2,\"delivered_t1_wh\":4821377,\"delivered_t2_wh\":3907142,"
              "\"returned_t2_wh\":12,\"power_delivered_w\":412,\"power_returned_w\":0,"
              "\"phases\":[{\"voltage_dv\":2314,\"current_ma\":1000},{},{}]}}"));
}

void test_summer_time_is_two_hours_ahead(void)
{
    status_t s = full();
    s.p1.values.time = (p1_time_t){ 26, 7, 1, 9, 5, 0, true, true };
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"time\":\"2026-07-01T09:05:00+02:00\""));
}

void test_text_is_escaped(void)
{
    status_t s = full();
    s.firmware = "1.4.0-\"dirty\"";
    strcpy(s.p1.meter, "a\"b\x01");
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"firmware\":\"1.4.0-\\\"dirty\\\"\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"name\":\"a\\\"b\\u0001\""));
}

void test_long_text_is_cut(void)
{
    status_t s = full();
    s.board = "0123456789012345678901234567890123456789";
    status_json(&s, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"board\":\"01234567890123456789012345678901\","));
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

/* Every number at its largest and every character needing six to write. */
static status_t worst(void)
{
    static char text[STATUS_TEXT_MAX + 1];
    memset(text, 0x01, STATUS_TEXT_MAX);
    status_t s = { .firmware = text,
                   .board = text,
                   .uptime_s = UINT32_MAX,
                   .has_rssi = true,
                   .rssi = INT32_MIN,
                   .readers = UINT32_MAX,
                   .readers_max = UINT32_MAX };
    s.p1 = (p1_status_t){ .counts = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX },
                          .seen = true,
                          .age_ms = UINT32_MAX,
                          .rejected = true,
                          .rejected_age_ms = UINT32_MAX };
    memset(s.p1.meter, 0x01, P1_METER_NAME_MAX);
    p1_values_t *v = &s.p1.values;
    p1_num_t *nums[NUMS_MAX];
    size_t count = all_nums(v, nums);
    for (size_t i = 0; i < count; i++) {
        *nums[i] = num(UINT32_MAX);
    }
    v->time = (p1_time_t){ 255, 255, 255, 255, 255, 255, true, true };
    v->gas_time = v->time;
    return s;
}

void test_the_worst_case_fits(void)
{
    status_t s = worst();
    size_t n = status_json(&s, json, sizeof(json));
    TEST_ASSERT_GREATER_THAN_size_t(0, n);
    TEST_ASSERT_EQUAL_size_t(strlen(json), n);
    /* Not much more room than it needs. */
    TEST_ASSERT_LESS_THAN_size_t(128, STATUS_JSON_MAX - n);
}

void test_a_buffer_that_is_too_small_gives_nothing(void)
{
    status_t s = full();
    size_t needed = status_json(&s, json, sizeof(json));
    char small[STATUS_JSON_MAX];
    memset(small, 'x', sizeof(small));
    TEST_ASSERT_EQUAL_size_t(0, status_json(&s, small, needed)); /* no room for the NUL */
    TEST_ASSERT_EQUAL_CHAR('\0', small[0]);
    TEST_ASSERT_EQUAL_size_t(needed, status_json(&s, small, needed + 1));
    TEST_ASSERT_EQUAL_size_t(0, status_json(&s, small, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_status);
    RUN_TEST(test_nothing_seen_yet);
    RUN_TEST(test_values_are_ignored_until_a_telegram_was_seen);
    RUN_TEST(test_no_rejected_telegram_no_time_for_it);
    RUN_TEST(test_a_value_the_meter_does_not_send_is_left_out);
    RUN_TEST(test_summer_time_is_two_hours_ahead);
    RUN_TEST(test_text_is_escaped);
    RUN_TEST(test_long_text_is_cut);
    RUN_TEST(test_the_worst_case_fits);
    RUN_TEST(test_a_buffer_that_is_too_small_gives_nothing);
    return UNITY_END();
}
