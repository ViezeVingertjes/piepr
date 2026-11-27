#include <string.h>

#include <unity.h>

#include "p1_ident.h"

void setUp(void) {}
void tearDown(void) {}

static size_t ident(const char *telegram, char *out, size_t out_size)
{
    return p1_ident((const uint8_t *)telegram, strlen(telegram), out, out_size);
}

void test_first_line_without_the_slash(void)
{
    char out[40];
    TEST_ASSERT_EQUAL(
        16, ident("/ISK5\\2M550E-1012\r\n\r\n1-3:0.2.8(50)\r\n!ABCD\r\n", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("ISK5\\2M550E-1012", out);
}

void test_line_may_end_in_lf_only_or_at_the_end(void)
{
    char out[40];
    TEST_ASSERT_EQUAL(4, ident("/ABC5\n1-0:1.8.1", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("ABC5", out);
    TEST_ASSERT_EQUAL(3, ident("/XYZ", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("XYZ", out);
}

void test_cut_to_the_buffer(void)
{
    char out[6];
    TEST_ASSERT_EQUAL(5, ident("/ISK5\\2M550E-1012\r\n", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("ISK5\\", out);
}

void test_unprintable_bytes_become_question_marks(void)
{
    char out[16];
    TEST_ASSERT_EQUAL(5, ident("/A\x01"
                               "B\xffZ\r\n",
                               out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("A?B?Z", out);
}

void test_not_a_telegram(void)
{
    char out[8] = "junk";
    TEST_ASSERT_EQUAL(0, ident("ISK5\r\n", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_EQUAL(0, ident("", out, sizeof(out)));
    TEST_ASSERT_EQUAL(0, p1_ident(NULL, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL(0, ident("/\r\n", out, sizeof(out)));
}

void test_length_and_tiny_buffers_are_respected(void)
{
    char out[8];
    TEST_ASSERT_EQUAL(2, p1_ident((const uint8_t *)"/ABCDEF", 3, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("AB", out);
    char one[1] = { 'x' };
    TEST_ASSERT_EQUAL(0, ident("/ABC\r\n", one, sizeof(one)));
    TEST_ASSERT_EQUAL('\0', one[0]);
    TEST_ASSERT_EQUAL(0, ident("/ABC\r\n", one, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_line_without_the_slash);
    RUN_TEST(test_line_may_end_in_lf_only_or_at_the_end);
    RUN_TEST(test_cut_to_the_buffer);
    RUN_TEST(test_unprintable_bytes_become_question_marks);
    RUN_TEST(test_not_a_telegram);
    RUN_TEST(test_length_and_tiny_buffers_are_respected);
    return UNITY_END();
}
