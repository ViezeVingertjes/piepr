#include <string.h>

#include <unity.h>

#include "p1_parse.h"

void setUp(void) {}
void tearDown(void) {}

/* tools/p1sim.py --version 5 --stdout */
static const char DSMR5[] = "/PPR5\\SIMULATOR\r\n"
                            "\r\n"
                            "1-3:0.2.8(50)\r\n"
                            "0-0:1.0.0(261128150934W)\r\n"
                            "0-0:96.1.1(50494550522D53494D554C41544F52)\r\n"
                            "1-0:1.8.1(001234.567*kWh)\r\n"
                            "1-0:1.8.2(002345.678*kWh)\r\n"
                            "1-0:2.8.1(000000.000*kWh)\r\n"
                            "1-0:2.8.2(000000.000*kWh)\r\n"
                            "0-0:96.14.0(0002)\r\n"
                            "1-0:1.7.0(00.450*kW)\r\n"
                            "1-0:2.7.0(00.000*kW)\r\n"
                            "0-0:96.7.21(00004)\r\n"
                            "0-0:96.7.9(00002)\r\n"
                            "1-0:99.97.0(0)(0-0:96.7.19)\r\n"
                            "1-0:32.32.0(00000)\r\n"
                            "1-0:32.36.0(00000)\r\n"
                            "0-0:96.13.0()\r\n"
                            "1-0:32.7.0(230.0*V)\r\n"
                            "1-0:31.7.0(002*A)\r\n"
                            "1-0:21.7.0(00.450*kW)\r\n"
                            "1-0:22.7.0(00.000*kW)\r\n"
                            "0-1:24.1.0(003)\r\n"
                            "0-1:96.1.0(50494550522D53494D554C41544F52)\r\n"
                            "0-1:24.2.1(261128150000W)(20951.868*m3)\r\n"
                            "!C464\r\n";

/* A Belgian e-MUCS meter: three phases, current with decimals, gas in 24.2.3. */
static const char BELGIAN[] = "/FLU5\\253769484_A\r\n"
                              "\r\n"
                              "0-0:96.1.4(50217)\r\n"
                              "0-0:1.0.0(200512135409S)\r\n"
                              "1-0:1.8.1(000000.034*kWh)\r\n"
                              "1-0:1.8.2(000015.758*kWh)\r\n"
                              "1-0:2.8.1(000000.000*kWh)\r\n"
                              "1-0:2.8.2(000000.011*kWh)\r\n"
                              "0-0:96.14.0(0001)\r\n"
                              "1-0:1.7.0(01.193*kW)\r\n"
                              "1-0:2.7.0(00.000*kW)\r\n"
                              "1-0:21.7.0(00.401*kW)\r\n"
                              "1-0:41.7.0(00.392*kW)\r\n"
                              "1-0:61.7.0(00.400*kW)\r\n"
                              "1-0:22.7.0(00.000*kW)\r\n"
                              "1-0:42.7.0(00.000*kW)\r\n"
                              "1-0:62.7.0(00.000*kW)\r\n"
                              "1-0:32.7.0(234.7*V)\r\n"
                              "1-0:52.7.0(233.9*V)\r\n"
                              "1-0:72.7.0(235.2*V)\r\n"
                              "1-0:31.7.0(001.75*A)\r\n"
                              "1-0:51.7.0(001.68*A)\r\n"
                              "1-0:71.7.0(001.70*A)\r\n"
                              "0-0:96.3.10(1)\r\n"
                              "0-0:17.0.0(999.9*kW)\r\n"
                              "1-0:31.4.0(999*A)\r\n"
                              "0-0:96.13.0()\r\n"
                              "0-1:24.1.0(003)\r\n"
                              "0-1:96.1.1(37464C4F32313139303333373333)\r\n"
                              "0-1:24.4.0(1)\r\n"
                              "0-1:24.2.3(200512134558S)(00112.384*m3)\r\n"
                              "!0000\r\n";

static size_t parse(const char *telegram, p1_values_t *v)
{
    return p1_parse((const uint8_t *)telegram, strlen(telegram), v);
}

/* None of these telegrams gives a value. */
static void assert_nothing_found(const char *const *telegrams, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        p1_values_t v;
        TEST_ASSERT_EQUAL_MESSAGE(0, parse(telegrams[i], &v), telegrams[i]);
    }
}

static void assert_num(uint32_t expected, p1_num_t n)
{
    TEST_ASSERT_TRUE(n.present);
    TEST_ASSERT_EQUAL_UINT32(expected, n.value);
}

void test_dsmr5_telegram(void)
{
    p1_values_t v;
    TEST_ASSERT_EQUAL(14, parse(DSMR5, &v));

    TEST_ASSERT_TRUE(v.time.present);
    TEST_ASSERT_EQUAL(26, v.time.year);
    TEST_ASSERT_EQUAL(11, v.time.month);
    TEST_ASSERT_EQUAL(28, v.time.day);
    TEST_ASSERT_EQUAL(15, v.time.hour);
    TEST_ASSERT_EQUAL(9, v.time.minute);
    TEST_ASSERT_EQUAL(34, v.time.second);
    TEST_ASSERT_FALSE(v.time.summer);

    assert_num(50, v.version);
    assert_num(2, v.tariff);
    assert_num(1234567, v.delivered_wh[0]);
    assert_num(2345678, v.delivered_wh[1]);
    assert_num(0, v.returned_wh[0]);
    assert_num(0, v.returned_wh[1]);
    assert_num(450, v.power_delivered_w);
    assert_num(0, v.power_returned_w);
    assert_num(450, v.phase_delivered_w[0]);
    assert_num(0, v.phase_returned_w[0]);
    assert_num(2300, v.voltage_dv[0]);
    assert_num(2000, v.current_ma[0]);

    assert_num(20951868, v.gas_l);
    TEST_ASSERT_TRUE(v.gas_time.present);
    TEST_ASSERT_EQUAL(15, v.gas_time.hour);
    TEST_ASSERT_EQUAL(0, v.gas_time.minute);
}

void test_values_a_meter_doesnt_send_are_absent(void)
{
    p1_values_t v;
    parse(DSMR5, &v);
    for (int i = 1; i < P1_PHASES; i++) {
        TEST_ASSERT_FALSE(v.phase_delivered_w[i].present);
        TEST_ASSERT_FALSE(v.phase_returned_w[i].present);
        TEST_ASSERT_FALSE(v.voltage_dv[i].present);
        TEST_ASSERT_FALSE(v.current_ma[i].present);
    }
}

void test_dsmr4_has_no_voltage(void)
{
    /* tools/p1sim.py --version 4 --stdout: no 32.7.0 */
    p1_values_t v;
    parse("/PPR4\\SIMULATOR\r\n\r\n1-3:0.2.8(42)\r\n1-0:31.7.0(002*A)\r\n!1076\r\n", &v);
    assert_num(42, v.version);
    assert_num(2000, v.current_ma[0]);
    TEST_ASSERT_FALSE(v.voltage_dv[0].present);
}

void test_belgian_telegram(void)
{
    p1_values_t v;
    TEST_ASSERT_EQUAL(21, parse(BELGIAN, &v));
    TEST_ASSERT_TRUE(v.time.summer);
    TEST_ASSERT_FALSE(v.version.present); /* Belgian meters send 0-0:96.1.4 instead */
    assert_num(1, v.tariff);
    assert_num(34, v.delivered_wh[0]);
    assert_num(15758, v.delivered_wh[1]);
    assert_num(11, v.returned_wh[1]);
    assert_num(1193, v.power_delivered_w);
    assert_num(401, v.phase_delivered_w[0]);
    assert_num(392, v.phase_delivered_w[1]);
    assert_num(400, v.phase_delivered_w[2]);
    assert_num(0, v.phase_returned_w[2]);
    assert_num(2347, v.voltage_dv[0]);
    assert_num(2339, v.voltage_dv[1]);
    assert_num(2352, v.voltage_dv[2]);
    assert_num(1750, v.current_ma[0]);
    assert_num(1680, v.current_ma[1]);
    assert_num(1700, v.current_ma[2]);
    assert_num(112384, v.gas_l);
    TEST_ASSERT_EQUAL(13, v.gas_time.hour);
    TEST_ASSERT_EQUAL(45, v.gas_time.minute);
    TEST_ASSERT_EQUAL(58, v.gas_time.second);
}

void test_lf_only_and_no_final_newline(void)
{
    p1_values_t v;
    TEST_ASSERT_EQUAL(2, parse("/X\n1-0:1.7.0(01.000*kW)\n1-0:2.7.0(00.250*kW)", &v));
    assert_num(1000, v.power_delivered_w);
    assert_num(250, v.power_returned_w);
}

void test_a_bad_line_spoils_only_itself(void)
{
    p1_values_t v;
    parse("/X\r\n"
          "1-0:1.8.1(00123x.567*kWh)\r\n" /* not a number */
          "1-0:1.8.2(002345.678*kWh)\r\n",
          &v);
    TEST_ASSERT_FALSE(v.delivered_wh[0].present);
    assert_num(2345678, v.delivered_wh[1]);
}

void test_unit_has_to_match(void)
{
    p1_values_t v;
    parse("/X\r\n"
          "1-0:1.7.0(450*W)\r\n"       /* wrong unit */
          "1-0:2.7.0(00.100)\r\n"      /* no unit */
          "1-0:32.7.0(230.0*v)\r\n"    /* case doesn't matter */
          "1-3:0.2.8(50*kWh)\r\n"      /* a unit where none belongs */
          "1-0:1.8.1(000001.000*)\r\n" /* empty unit */
          "1-0:1.8.2(000001.000*kWhh)\r\n",
          &v);
    TEST_ASSERT_FALSE(v.power_delivered_w.present);
    TEST_ASSERT_FALSE(v.power_returned_w.present);
    assert_num(2300, v.voltage_dv[0]);
    TEST_ASSERT_FALSE(v.version.present);
    TEST_ASSERT_FALSE(v.delivered_wh[0].present);
    TEST_ASSERT_FALSE(v.delivered_wh[1].present);
}

void test_decimals_are_filled_up_or_cut_off(void)
{
    p1_values_t v;
    parse("/X\r\n"
          "1-0:1.7.0(1*kW)\r\n"
          "1-0:2.7.0(0.1234*kW)\r\n"
          "1-0:32.7.0(229*V)\r\n",
          &v);
    assert_num(1000, v.power_delivered_w);
    assert_num(123, v.power_returned_w);
    assert_num(2290, v.voltage_dv[0]);
}

void test_malformed_numbers(void)
{
    const char *bad[] = {
        "/X\r\n1-0:1.7.0(*kW)\r\n",
        "/X\r\n1-0:1.7.0(.5*kW)\r\n",
        "/X\r\n1-0:1.7.0(1.*kW)\r\n",
        "/X\r\n1-0:1.7.0()\r\n",
        "/X\r\n1-0:1.7.0(01.000*kW\r\n",
        "/X\r\n1-0:1.7.0 (01.000*kW)\r\n",
        "/X\r\n1-0:1.7.0(-1.000*kW)\r\n",
        "/X\r\n1-0:1.7.0(1.0.0*kW)\r\n",
        "/X\r\n1-0:1.7.0(4294968.000*kW)\r\n",
    };
    assert_nothing_found(bad, sizeof(bad) / sizeof(bad[0]));
}

void test_largest_number(void)
{
    p1_values_t v;
    parse("/X\r\n1-0:1.7.0(4294967.295*kW)\r\n1-0:2.7.0(4294967.296*kW)\r\n", &v);
    assert_num(UINT32_MAX, v.power_delivered_w);
    TEST_ASSERT_FALSE(v.power_returned_w.present);
}

void test_bad_timestamps(void)
{
    const char *bad[] = {
        "/X\r\n0-0:1.0.0(261128150934)\r\n",   "/X\r\n0-0:1.0.0(261128150934X)\r\n",
        "/X\r\n0-0:1.0.0(261328150934W)\r\n",  "/X\r\n0-0:1.0.0(261100150934W)\r\n",
        "/X\r\n0-0:1.0.0(261132150934W)\r\n",  "/X\r\n0-0:1.0.0(261128240934W)\r\n",
        "/X\r\n0-0:1.0.0(261128156034W)\r\n",  "/X\r\n0-0:1.0.0(261128150960W)\r\n",
        "/X\r\n0-0:1.0.0(2611281509340W)\r\n", "/X\r\n0-0:1.0.0(26112815093aW)\r\n",
    };
    assert_nothing_found(bad, sizeof(bad) / sizeof(bad[0]));
}

void test_gas_from_the_gas_meter_only(void)
{
    p1_values_t v;
    /* A water meter (type 7) on channel 1, gas on channel 2. */
    parse("/X\r\n"
          "0-1:24.1.0(007)\r\n"
          "0-1:24.2.1(261128150000W)(00123.456*m3)\r\n"
          "0-2:24.1.0(003)\r\n"
          "0-2:24.2.1(261128151000W)(20951.868*m3)\r\n",
          &v);
    assert_num(20951868, v.gas_l);
    TEST_ASSERT_EQUAL(10, v.gas_time.minute);
}

void test_gas_without_device_type_and_first_one_wins(void)
{
    p1_values_t v;
    parse("/X\r\n"
          "0-3:24.2.1(261128150000W)(00001.000*m3)\r\n"
          "0-4:24.2.1(261128150000W)(00002.000*m3)\r\n",
          &v);
    assert_num(1000, v.gas_l);
}

void test_bad_gas_lines(void)
{
    const char *bad[] = {
        "/X\r\n0-1:24.2.1(261128150000W)\r\n",                /* no reading */
        "/X\r\n0-1:24.2.1(261128150000W)(00001.000*kWh)\r\n", /* wrong unit */
        "/X\r\n0-1:24.2.1(2611281500W)(00001.000*m3)\r\n",    /* bad time */
        "/X\r\n0-5:24.2.1(261128150000W)(00001.000*m3)\r\n",  /* no channel 5 */
        "/X\r\n0-0:24.2.1(261128150000W)(00001.000*m3)\r\n",  /* nor channel 0 */
        "/X\r\n0-1:24.2.2(261128150000W)(00001.000*m3)\r\n",  /* not a gas reading */
    };
    assert_nothing_found(bad, sizeof(bad) / sizeof(bad[0]));
}

void test_lines_that_look_alike_are_not_mixed_up(void)
{
    p1_values_t v;
    TEST_ASSERT_EQUAL(0, parse("/X\r\n"
                               "1-0:1.8.10(000001.000*kWh)\r\n"
                               "1-0:1.8(000001.000*kWh)\r\n"
                               "0-0:96.14.0.1(0001)\r\n"
                               "1-0:32.32.0(00000)\r\n"
                               "0-0:1.0.0.0(261128150934W)\r\n",
                               &v));
}

void test_later_line_replaces_earlier(void)
{
    p1_values_t v;
    TEST_ASSERT_EQUAL(1, parse("/X\r\n1-0:1.7.0(01.000*kW)\r\n1-0:1.7.0(02.000*kW)\r\n", &v));
    assert_num(2000, v.power_delivered_w);
}

void test_nothing_to_parse(void)
{
    p1_values_t v;
    memset(&v, 0xAA, sizeof(v));
    TEST_ASSERT_EQUAL(0, p1_parse(NULL, 0, &v));
    TEST_ASSERT_FALSE(v.time.present);
    TEST_ASSERT_FALSE(v.gas_l.present);
    TEST_ASSERT_EQUAL(0, parse("", &v));
    TEST_ASSERT_EQUAL(0, parse("\r\n\r\n(", &v));
    /* Only the first `len` bytes count. */
    TEST_ASSERT_EQUAL(0, p1_parse((const uint8_t *)"1-0:1.7.0(01.000*kW)", 19, &v));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dsmr5_telegram);
    RUN_TEST(test_values_a_meter_doesnt_send_are_absent);
    RUN_TEST(test_dsmr4_has_no_voltage);
    RUN_TEST(test_belgian_telegram);
    RUN_TEST(test_lf_only_and_no_final_newline);
    RUN_TEST(test_a_bad_line_spoils_only_itself);
    RUN_TEST(test_unit_has_to_match);
    RUN_TEST(test_decimals_are_filled_up_or_cut_off);
    RUN_TEST(test_malformed_numbers);
    RUN_TEST(test_largest_number);
    RUN_TEST(test_bad_timestamps);
    RUN_TEST(test_gas_from_the_gas_meter_only);
    RUN_TEST(test_gas_without_device_type_and_first_one_wins);
    RUN_TEST(test_bad_gas_lines);
    RUN_TEST(test_lines_that_look_alike_are_not_mixed_up);
    RUN_TEST(test_later_line_replaces_earlier);
    RUN_TEST(test_nothing_to_parse);
    return UNITY_END();
}
