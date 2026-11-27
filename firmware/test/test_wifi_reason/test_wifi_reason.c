#include <stddef.h>
#include <string.h>

#include <unity.h>

#include "wifi_reason.h"

void setUp(void) {}
void tearDown(void) {}

void test_reasons_a_user_can_act_on(void)
{
    TEST_ASSERT_EQUAL_STRING("network not found", wifi_reason_text(201));
    TEST_ASSERT_EQUAL_STRING("password rejected", wifi_reason_text(202));
    TEST_ASSERT_EQUAL_STRING("password check timed out", wifi_reason_text(15));
    TEST_ASSERT_EQUAL_STRING("password check timed out", wifi_reason_text(204));
    TEST_ASSERT_EQUAL_STRING("access point stopped answering", wifi_reason_text(200));
    TEST_ASSERT_EQUAL_STRING("access point refused the connection", wifi_reason_text(203));
}

void test_other_known_reasons(void)
{
    TEST_ASSERT_EQUAL_STRING("sign-in expired", wifi_reason_text(2));
    TEST_ASSERT_EQUAL_STRING("left the network", wifi_reason_text(8));
    TEST_ASSERT_EQUAL_STRING("could not connect", wifi_reason_text(205));
}

void test_unknown_reasons_have_no_text(void)
{
    TEST_ASSERT_NULL(wifi_reason_text(0));
    TEST_ASSERT_NULL(wifi_reason_text(1));
    TEST_ASSERT_NULL(wifi_reason_text(199));
    TEST_ASSERT_NULL(wifi_reason_text(206));
    TEST_ASSERT_NULL(wifi_reason_text(65535));
}

void test_texts_fit_one_log_line(void)
{
    for (unsigned reason = 0; reason < 300; reason++) {
        const char *text = wifi_reason_text(reason);
        if (text != NULL) {
            TEST_ASSERT_TRUE(strlen(text) > 0 && strlen(text) <= 40);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reasons_a_user_can_act_on);
    RUN_TEST(test_other_known_reasons);
    RUN_TEST(test_unknown_reasons_have_no_text);
    RUN_TEST(test_texts_fit_one_log_line);
    return UNITY_END();
}
