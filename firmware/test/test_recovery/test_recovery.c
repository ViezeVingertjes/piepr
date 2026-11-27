#include <unity.h>

#include "recovery.h"

void setUp(void) {}
void tearDown(void) {}

void test_fresh_boot_none(void)
{
    TEST_ASSERT_EQUAL(SETUP_REASON_NONE, recovery_check(false, 0, 0));
}

void test_timeout_boundary(void)
{
    TEST_ASSERT_EQUAL(SETUP_REASON_NONE, recovery_check(false, 0, 299));
    TEST_ASSERT_EQUAL(SETUP_REASON_NO_CONNECT, recovery_check(false, 0, 300));
}

void test_auth_boundary(void)
{
    TEST_ASSERT_EQUAL(SETUP_REASON_NONE, recovery_check(false, 2, 10));
    TEST_ASSERT_EQUAL(SETUP_REASON_AUTH, recovery_check(false, 3, 10));
}

void test_auth_wins_over_timeout(void)
{
    TEST_ASSERT_EQUAL(SETUP_REASON_AUTH, recovery_check(false, 3, 400));
}

void test_connected_suppresses(void)
{
    TEST_ASSERT_EQUAL(SETUP_REASON_NONE, recovery_check(true, 5, 10000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fresh_boot_none);
    RUN_TEST(test_timeout_boundary);
    RUN_TEST(test_auth_boundary);
    RUN_TEST(test_auth_wins_over_timeout);
    RUN_TEST(test_connected_suppresses);
    return UNITY_END();
}
