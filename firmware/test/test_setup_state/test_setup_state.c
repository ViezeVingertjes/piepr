#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "setup_state.h"

void setUp(void) {}
void tearDown(void) {}

static setup_network_t list[SETUP_SCAN_LIST_MAX];
static size_t count;

static void add(const char *ssid, int8_t rssi, bool secure)
{
    setup_networks_add(list, &count, ssid, strlen(ssid), rssi, secure);
}

static const char *net_name(int i)
{
    static char ssid[8];
    snprintf(ssid, sizeof(ssid), "net%d", i);
    return ssid;
}

/* net0 strongest down to the weakest, last entry. */
static void fill_list(void)
{
    count = 0;
    for (int i = 0; i < SETUP_SCAN_LIST_MAX; i++) {
        add(net_name(i), (int8_t)(-40 - i), true);
    }
}

void test_add_sorted(void)
{
    count = 0;
    add("a", -70, true);
    add("b", -50, true);
    add("c", -60, true);
    TEST_ASSERT_EQUAL_size_t(3, count);
    TEST_ASSERT_EQUAL_STRING("b", list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("c", list[1].ssid);
    TEST_ASSERT_EQUAL_STRING("a", list[2].ssid);
}

void test_add_merges_duplicate_keeps_strongest(void)
{
    count = 0;
    add("Mesh", -70, false);
    add("Mesh", -40, true);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_INT8(-40, list[0].rssi);
    TEST_ASSERT_TRUE(list[0].secure);
}

void test_add_ignores_weaker_duplicate(void)
{
    count = 0;
    add("Mesh", -40, true);
    add("x", -50, true);
    add("Mesh", -70, false);
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_STRING("Mesh", list[0].ssid);
    TEST_ASSERT_EQUAL_INT8(-40, list[0].rssi);
    TEST_ASSERT_TRUE(list[0].secure);
}

void test_add_ignores_hidden(void)
{
    count = 0;
    setup_networks_add(list, &count, "", 0, -40, true);
    TEST_ASSERT_EQUAL_size_t(0, count);
}

void test_add_min_rssi_boundary(void)
{
    count = 0;
    add("weak", SETUP_SCAN_MIN_RSSI - 1, true);
    add("edge", SETUP_SCAN_MIN_RSSI, true);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING("edge", list[0].ssid);
}

void test_add_ignores_too_long(void)
{
    count = 0;
    setup_networks_add(list, &count, "123456789012345678901234567890123", 33, -40, true);
    TEST_ASSERT_EQUAL_size_t(0, count);
}

void test_add_ignores_invalid_utf8(void)
{
    /* A Latin-1 name can't be shown or posted back unchanged by the page. */
    count = 0;
    setup_networks_add(list, &count, "Caf\xE9", 4, -40, true);
    TEST_ASSERT_EQUAL_size_t(0, count);
}

void test_add_accepts_max_len_unterminated(void)
{
    char ssid[32];
    memset(ssid, 'x', sizeof(ssid));
    count = 0;
    setup_networks_add(list, &count, ssid, sizeof(ssid), -40, true);
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_size_t(32, strlen(list[0].ssid));
}

void test_add_full_evicts_weakest(void)
{
    fill_list();
    add("strong", -30, true);
    TEST_ASSERT_EQUAL_size_t(SETUP_SCAN_LIST_MAX, count);
    TEST_ASSERT_EQUAL_STRING("strong", list[0].ssid);
    TEST_ASSERT_EQUAL_STRING(net_name(SETUP_SCAN_LIST_MAX - 2), list[SETUP_SCAN_LIST_MAX - 1].ssid);
}

void test_add_full_drops_weaker(void)
{
    fill_list();
    add("weak", -90, true);
    TEST_ASSERT_EQUAL_size_t(SETUP_SCAN_LIST_MAX, count);
    TEST_ASSERT_EQUAL_STRING(net_name(SETUP_SCAN_LIST_MAX - 1), list[SETUP_SCAN_LIST_MAX - 1].ssid);
}

static char json[SETUP_STATE_JSON_MAX];

#define SURVEY_MAX 4
static setup_survey_entry_t survey[SURVEY_MAX];
static size_t surveyed;

static void reading(uint8_t scan, const char *ssid, int8_t rssi)
{
    setup_survey_add(survey, &surveyed, SURVEY_MAX, scan, ssid, strlen(ssid), rssi, true);
}

static void survey_result(void)
{
    count = 0;
    setup_survey_result(survey, surveyed, list, &count);
}

void test_survey_averages_over_scans(void)
{
    surveyed = 0;
    reading(0, "a", -50);
    reading(1, "a", -70);
    reading(2, "a", -60);
    survey_result();
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_INT8(-60, list[0].rssi);
}

void test_survey_momentary_dip_keeps_rank(void)
{
    surveyed = 0;
    reading(0, "usual", -50);
    reading(0, "steady", -60);
    reading(1, "usual", -75); /* one bad moment */
    reading(1, "steady", -60);
    reading(2, "usual", -50);
    reading(2, "steady", -60);
    survey_result();
    TEST_ASSERT_EQUAL_STRING("usual", list[0].ssid);
    TEST_ASSERT_EQUAL_STRING("steady", list[1].ssid);
}

void test_survey_counts_strongest_per_scan(void)
{
    surveyed = 0;
    reading(0, "mesh", -70);
    reading(0, "mesh", -50); /* second access point, same scan */
    reading(1, "mesh", -50);
    survey_result();
    TEST_ASSERT_EQUAL_UINT8(2, survey[0].scans);
    TEST_ASSERT_EQUAL_INT8(-50, list[0].rssi);
}

void test_survey_averages_only_scans_that_saw_it(void)
{
    surveyed = 0;
    reading(0, "a", -60);
    reading(2, "a", -70);
    survey_result();
    TEST_ASSERT_EQUAL_INT8(-65, list[0].rssi);
}

void test_survey_full_still_counts_known_names(void)
{
    surveyed = 0;
    for (int i = 0; i < SURVEY_MAX; i++) {
        reading(0, net_name(i), -50);
    }
    reading(1, net_name(0), -50);
    TEST_ASSERT_EQUAL_UINT8(2, survey[0].scans);
}

void test_survey_full_replaces_weakest(void)
{
    surveyed = 0;
    for (int i = 0; i < SURVEY_MAX; i++) {
        reading(0, net_name(i), (int8_t)(-60 - i)); /* last one weakest */
    }
    reading(0, "home", -45);
    TEST_ASSERT_EQUAL_size_t(SURVEY_MAX, surveyed);
    survey_result();
    TEST_ASSERT_EQUAL_STRING("home", list[0].ssid);
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT_NOT_EQUAL(0, strcmp(net_name(SURVEY_MAX - 1), list[i].ssid));
    }
}

void test_survey_full_drops_weaker_new_name(void)
{
    surveyed = 0;
    for (int i = 0; i < SURVEY_MAX; i++) {
        reading(0, net_name(i), -60);
    }
    reading(0, "far", -75);
    survey_result();
    for (size_t i = 0; i < count; i++) {
        TEST_ASSERT_NOT_EQUAL(0, strcmp("far", list[i].ssid));
    }
}

void test_survey_secure_if_any_access_point_is(void)
{
    surveyed = 0;
    setup_survey_add(survey, &surveyed, SURVEY_MAX, 0, "Home", 4, -80, false);
    setup_survey_add(survey, &surveyed, SURVEY_MAX, 0, "Home", 4, -50, true);
    survey_result();
    TEST_ASSERT_TRUE(list[0].secure);
}

void test_survey_average_rounds_down(void)
{
    surveyed = 0;
    reading(0, "edge", SETUP_SCAN_MIN_RSSI);
    reading(1, "edge", SETUP_SCAN_MIN_RSSI - 1); /* mean is half a dB below the limit */
    survey_result();
    TEST_ASSERT_EQUAL_size_t(0, count);
}

void test_survey_ignores_invalid_names(void)
{
    char longname[WIFI_SSID_MAX_LEN + 1];
    memset(longname, 'x', sizeof(longname));
    surveyed = 0;
    setup_survey_add(survey, &surveyed, SURVEY_MAX, 0, "", 0, -50, true);
    setup_survey_add(survey, &surveyed, SURVEY_MAX, 0, longname, sizeof(longname), -50, true);
    TEST_ASSERT_EQUAL_size_t(0, surveyed);
}

void test_survey_min_rssi_applies_to_average(void)
{
    surveyed = 0;
    reading(0, "flaky", SETUP_SCAN_MIN_RSSI + 5);
    reading(1, "flaky", SETUP_SCAN_MIN_RSSI - 15);
    survey_result();
    TEST_ASSERT_EQUAL_size_t(0, count);
}

void test_json_empty(void)
{
    size_t n = setup_state_json(SETUP_REASON_NONE, NULL, NULL, 0, false, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING("{\"reason\":\"none\",\"reason_ssid\":\"\",\"scanning\":false,\"networks\":[]}", json);
    TEST_ASSERT_EQUAL_size_t(strlen(json), n);
}

void test_json_reason_and_nets(void)
{
    count = 0;
    add("Home", -60, true);
    add("Cafe", -80, false);
    setup_state_json(SETUP_REASON_AUTH, "Home", list, count, false, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING("{\"reason\":\"auth\",\"reason_ssid\":\"Home\",\"scanning\":false,\"networks\":["
                             "{\"ssid\":\"Home\",\"rssi\":-60,\"secure\":true},"
                             "{\"ssid\":\"Cafe\",\"rssi\":-80,\"secure\":false}]}",
                             json);
}

void test_json_no_connect_reason(void)
{
    setup_state_json(SETUP_REASON_NO_CONNECT, "Gone", NULL, 0, false, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING("{\"reason\":\"no_connect\",\"reason_ssid\":\"Gone\",\"scanning\":false,\"networks\":[]}",
                             json);
}

void test_json_scanning(void)
{
    setup_state_json(SETUP_REASON_NONE, NULL, NULL, 0, true, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"scanning\":true"));
}

void test_json_escapes_ssid(void)
{
    count = 0;
    add("a\"b", -60, true);
    setup_state_json(SETUP_REASON_NONE, NULL, list, count, false, json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"ssid\":\"a\\\"b\""));
}

void test_json_worst_case_fits(void)
{
    static setup_network_t worst[SETUP_SCAN_LIST_MAX];
    char ssid[WIFI_SSID_MAX_LEN + 1];
    memset(ssid, 0x01, WIFI_SSID_MAX_LEN);
    ssid[WIFI_SSID_MAX_LEN] = '\0';
    for (int i = 0; i < SETUP_SCAN_LIST_MAX; i++) {
        memcpy(worst[i].ssid, ssid, sizeof(ssid));
        worst[i].rssi = -128;
        worst[i].secure = false;
    }
    size_t n = setup_state_json(SETUP_REASON_NO_CONNECT, ssid, worst, SETUP_SCAN_LIST_MAX, false, json,
                                sizeof(json));
    TEST_ASSERT_TRUE(n > 0 && n < SETUP_STATE_JSON_MAX);
}

void test_json_too_small(void)
{
    char small[16] = "x";
    TEST_ASSERT_EQUAL_size_t(
        0, setup_state_json(SETUP_REASON_NONE, NULL, NULL, 0, false, small, sizeof(small)));
    TEST_ASSERT_EQUAL_CHAR('\0', small[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_sorted);
    RUN_TEST(test_add_merges_duplicate_keeps_strongest);
    RUN_TEST(test_add_ignores_weaker_duplicate);
    RUN_TEST(test_add_ignores_hidden);
    RUN_TEST(test_add_min_rssi_boundary);
    RUN_TEST(test_add_ignores_too_long);
    RUN_TEST(test_add_ignores_invalid_utf8);
    RUN_TEST(test_add_accepts_max_len_unterminated);
    RUN_TEST(test_add_full_evicts_weakest);
    RUN_TEST(test_add_full_drops_weaker);
    RUN_TEST(test_survey_averages_over_scans);
    RUN_TEST(test_survey_momentary_dip_keeps_rank);
    RUN_TEST(test_survey_counts_strongest_per_scan);
    RUN_TEST(test_survey_averages_only_scans_that_saw_it);
    RUN_TEST(test_survey_full_still_counts_known_names);
    RUN_TEST(test_survey_full_replaces_weakest);
    RUN_TEST(test_survey_full_drops_weaker_new_name);
    RUN_TEST(test_survey_secure_if_any_access_point_is);
    RUN_TEST(test_survey_average_rounds_down);
    RUN_TEST(test_survey_ignores_invalid_names);
    RUN_TEST(test_survey_min_rssi_applies_to_average);
    RUN_TEST(test_json_empty);
    RUN_TEST(test_json_reason_and_nets);
    RUN_TEST(test_json_no_connect_reason);
    RUN_TEST(test_json_scanning);
    RUN_TEST(test_json_escapes_ssid);
    RUN_TEST(test_json_worst_case_fits);
    RUN_TEST(test_json_too_small);
    return UNITY_END();
}
