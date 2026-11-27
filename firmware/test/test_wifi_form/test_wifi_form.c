#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "wifi_form.h"

static wifi_credentials_t c;

void setUp(void)
{
    memset(&c, 0, sizeof(c));
}
void tearDown(void) {}

static wifi_form_result_t parse(const char *body)
{
    return wifi_form_parse(body, strlen(body), &c);
}

/* "ssid=<n x 'a'>&pass=x" etc. */
static const char *repeat(char ch, size_t n)
{
    static char buf[256];
    memset(buf, ch, n);
    buf[n] = '\0';
    return buf;
}

static wifi_form_result_t parse_lengths(size_t ssid_len, size_t pass_len)
{
    char body[300];
    char ssid[100];
    strcpy(ssid, repeat('s', ssid_len));
    snprintf(body, sizeof(body), "ssid=%s&pass=%s", ssid, repeat('p', pass_len));
    return parse(body);
}

void test_plain_ok(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse("ssid=Home&pass=secret123"));
    TEST_ASSERT_EQUAL_STRING("Home", c.ssid);
    TEST_ASSERT_EQUAL_STRING("secret123", c.pass);
}

void test_plus_and_percent_decoded(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse("ssid=My+Home%21&pass=a%26b%3Dc%25dXY"));
    TEST_ASSERT_EQUAL_STRING("My Home!", c.ssid);
    TEST_ASSERT_EQUAL_STRING("a&b=c%dXY", c.pass);
}

void test_percent_lowercase(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse("ssid=a%2fb&pass="));
    TEST_ASSERT_EQUAL_STRING("a/b", c.ssid);
}

void test_utf8_ssid(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse("ssid=Caf%C3%A9&pass="));
    TEST_ASSERT_EQUAL_STRING("Caf\xC3\xA9", c.ssid);
}

void test_bad_escape_malformed(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=a%G1&pass="));
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=a%4&pass="));
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=a&pass=b%"));
}

void test_nul_byte_malformed(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=a%00b&pass="));
}

void test_missing_key_malformed(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=Home"));
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("pass=secret123"));
}

void test_duplicate_key_malformed(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse("ssid=A&ssid=B&pass="));
}

void test_unknown_key_ignored(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse("x=1&ssid=Home&submit=Save&pass="));
    TEST_ASSERT_EQUAL_STRING("Home", c.ssid);
    TEST_ASSERT_EQUAL_STRING("", c.pass);
}

void test_empty_body_malformed(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_MALFORMED, parse(""));
}

void test_ssid_length_bounds(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_BAD_SSID, parse_lengths(0, 8));
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse_lengths(1, 8));
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse_lengths(32, 8));
    TEST_ASSERT_EQUAL(WIFI_FORM_BAD_SSID, parse_lengths(33, 8));

    /* 32 bytes written as %xx escapes (96 characters) is still 32 bytes. */
    char body[200] = "ssid=";
    for (int i = 0; i < 32; i++) {
        strcat(body, "%41");
    }
    strcat(body, "&pass=");
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse(body));
    TEST_ASSERT_EQUAL_STRING(repeat('A', 32), c.ssid);
}

void test_pass_length_bounds(void)
{
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse_lengths(4, 0));
    TEST_ASSERT_EQUAL(WIFI_FORM_BAD_PASS, parse_lengths(4, 7));
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse_lengths(4, 8));
    TEST_ASSERT_EQUAL(WIFI_FORM_OK, parse_lengths(4, 63));
    TEST_ASSERT_EQUAL(WIFI_FORM_BAD_PASS, parse_lengths(4, 64));
}

void test_out_untouched_on_error(void)
{
    memset(&c, 'Z', sizeof(c));
    TEST_ASSERT_EQUAL(WIFI_FORM_BAD_PASS, parse("ssid=Home&pass=short"));
    for (size_t i = 0; i < sizeof(c); i++) {
        TEST_ASSERT_EQUAL_HEX8('Z', ((unsigned char *)&c)[i]);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_plain_ok);
    RUN_TEST(test_plus_and_percent_decoded);
    RUN_TEST(test_percent_lowercase);
    RUN_TEST(test_utf8_ssid);
    RUN_TEST(test_bad_escape_malformed);
    RUN_TEST(test_nul_byte_malformed);
    RUN_TEST(test_missing_key_malformed);
    RUN_TEST(test_duplicate_key_malformed);
    RUN_TEST(test_unknown_key_ignored);
    RUN_TEST(test_empty_body_malformed);
    RUN_TEST(test_ssid_length_bounds);
    RUN_TEST(test_pass_length_bounds);
    RUN_TEST(test_out_untouched_on_error);
    return UNITY_END();
}
