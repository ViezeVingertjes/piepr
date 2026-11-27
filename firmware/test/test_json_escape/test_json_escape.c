#include <string.h>
#include <unity.h>

#include "json_escape.h"

#define REPLACEMENT "\xEF\xBF\xBD"

void setUp(void) {}
void tearDown(void) {}

static char out[256];

static void assert_escapes(const char *in, size_t len, const char *expected)
{
    TEST_ASSERT_TRUE(json_escape(in, len, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

void test_plain_ascii(void)
{
    assert_escapes("HomeNetwork", 11, "HomeNetwork");
}

void test_quote_backslash(void)
{
    assert_escapes("a\"b\\c", 5, "a\\\"b\\\\c");
}

void test_control_bytes(void)
{
    assert_escapes("\x01\n\x7f", 3, "\\u0001\\u000a\\u007f");
}

void test_embedded_nul(void)
{
    const char in[] = { 'a', 0, 'b' };
    assert_escapes(in, sizeof(in), "a\\u0000b");
}

void test_valid_utf8_passes(void)
{
    const char *in = "caf\xC3\xA9 \xF0\x9F\x93\xB6";
    assert_escapes(in, strlen(in), in);
}

void test_invalid_byte(void)
{
    assert_escapes("\xff", 1, REPLACEMENT);
}

void test_truncated_sequence(void)
{
    assert_escapes("a\xC3", 2, "a" REPLACEMENT);
}

void test_overlong(void)
{
    assert_escapes("\xC0\xAF", 2, REPLACEMENT REPLACEMENT);
}

void test_surrogate(void)
{
    assert_escapes("\xED\xA0\x80", 3, REPLACEMENT REPLACEMENT REPLACEMENT);
}

void test_above_max_code_point(void)
{
    assert_escapes("\xF4\x90\x80\x80", 4, REPLACEMENT REPLACEMENT REPLACEMENT REPLACEMENT);
}

void test_exact_fit(void)
{
    char small[sizeof("a\\\"b")];
    TEST_ASSERT_TRUE(json_escape("a\"b", 3, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("a\\\"b", small);
}

void test_too_small(void)
{
    char small[sizeof("a\\\"b") - 1] = "x";
    TEST_ASSERT_FALSE(json_escape("a\"b", 3, small, sizeof(small)));
    TEST_ASSERT_EQUAL_CHAR('\0', small[0]);
}

void test_worst_case_bound(void)
{
    char in[32];
    char big[JSON_ESCAPE_MAX(32)];
    memset(in, 0x01, sizeof(in));
    TEST_ASSERT_TRUE(json_escape(in, sizeof(in), big, sizeof(big)));
    TEST_ASSERT_EQUAL_size_t(32 * 6, strlen(big));
}

void test_utf8_valid(void)
{
    TEST_ASSERT_TRUE(utf8_valid("caf\xC3\xA9 \xF0\x9F\x93\xB6", 10));
    TEST_ASSERT_TRUE(utf8_valid("", 0));
    TEST_ASSERT_FALSE(utf8_valid("Caf\xE9", 4));
    TEST_ASSERT_FALSE(utf8_valid("\xED\xA0\x80", 3));
    TEST_ASSERT_FALSE(utf8_valid("a\xC3", 2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plain_ascii);
    RUN_TEST(test_quote_backslash);
    RUN_TEST(test_control_bytes);
    RUN_TEST(test_embedded_nul);
    RUN_TEST(test_valid_utf8_passes);
    RUN_TEST(test_invalid_byte);
    RUN_TEST(test_truncated_sequence);
    RUN_TEST(test_overlong);
    RUN_TEST(test_surrogate);
    RUN_TEST(test_above_max_code_point);
    RUN_TEST(test_exact_fit);
    RUN_TEST(test_too_small);
    RUN_TEST(test_worst_case_bound);
    RUN_TEST(test_utf8_valid);
    return UNITY_END();
}
