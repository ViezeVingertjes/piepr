#include <string.h>

#include <unity.h>

#include "lang_pick.h"

void setUp(void) {}
void tearDown(void) {}

static bool dutch(const char *header)
{
    return lang_prefers_dutch(header, strlen(header));
}

void test_no_header_is_english(void)
{
    TEST_ASSERT_FALSE(lang_prefers_dutch(NULL, 0));
    TEST_ASSERT_FALSE(dutch(""));
}

void test_dutch_phone(void)
{
    TEST_ASSERT_TRUE(dutch("nl"));
    TEST_ASSERT_TRUE(dutch("nl-NL,nl;q=0.9,en-US;q=0.8,en;q=0.7"));
    TEST_ASSERT_TRUE(dutch("nl-BE"));
}

void test_english_phone(void)
{
    TEST_ASSERT_FALSE(dutch("en-GB,en;q=0.9"));
    TEST_ASSERT_FALSE(dutch("en-US,en;q=0.9,nl;q=0.8"));
}

void test_other_first_language_then_the_higher_of_the_two(void)
{
    TEST_ASSERT_TRUE(dutch("fr-BE,fr;q=0.9,nl;q=0.8,en;q=0.7"));
    TEST_ASSERT_FALSE(dutch("fr-BE,fr;q=0.9,en;q=0.8,nl;q=0.7"));
    TEST_ASSERT_FALSE(dutch("fr,de;q=0.5"));
    TEST_ASSERT_FALSE(dutch("*"));
}

void test_equal_weight_first_listed_wins(void)
{
    TEST_ASSERT_TRUE(dutch("nl,en"));
    TEST_ASSERT_FALSE(dutch("en,nl"));
    TEST_ASSERT_TRUE(dutch("nl;q=0.5,en;q=0.5"));
    TEST_ASSERT_TRUE(dutch("en;q=0.1,nl;q=0.5,en;q=0.5"));
    TEST_ASSERT_FALSE(dutch("nl;q=0.1,en;q=0.5,nl;q=0.5"));
}

void test_case_and_spaces(void)
{
    TEST_ASSERT_TRUE(dutch("NL-nl"));
    TEST_ASSERT_TRUE(dutch(" nl ; q=0.5 , en ; q=0.4"));
    TEST_ASSERT_TRUE(dutch("nl;Q=0.9,en;q=0.8"));
}

void test_only_the_language_nl_counts(void)
{
    TEST_ASSERT_FALSE(dutch("nld"));
    TEST_ASSERT_FALSE(dutch("x-nl"));
    TEST_ASSERT_FALSE(dutch("fr-nl"));
}

void test_weight_zero_means_not_wanted(void)
{
    TEST_ASSERT_FALSE(dutch("nl;q=0"));
    TEST_ASSERT_FALSE(dutch("nl;q=0.000,en;q=0"));
    TEST_ASSERT_TRUE(dutch("nl;q=0.001,en;q=0"));
}

void test_bad_weights_are_ignored(void)
{
    TEST_ASSERT_FALSE(dutch("nl;q=abc,en;q=0.1"));
    TEST_ASSERT_FALSE(dutch("nl;q=1.5,en;q=0.1"));
    TEST_ASSERT_FALSE(dutch("nl;q=,en;q=0.1"));
    TEST_ASSERT_FALSE(dutch("nl;q=0.1234,en;q=0.1"));
    TEST_ASSERT_TRUE(dutch("nl;q=1.000,en;q=0.999"));
}

void test_cut_off_header(void)
{
    /* The portal reads the header into a fixed buffer: the tail may be missing. */
    const char *header = "nl-NL,nl;q=0.9,en-US;q=0.8";
    TEST_ASSERT_TRUE(lang_prefers_dutch(header, 5));
    TEST_ASSERT_TRUE(lang_prefers_dutch(header, 12));
    TEST_ASSERT_FALSE(lang_prefers_dutch("en-US,nl;q=0.9", 9));
}

void test_length_is_respected(void)
{
    TEST_ASSERT_FALSE(lang_prefers_dutch("en,nl-NL", 2));
    TEST_ASSERT_FALSE(lang_prefers_dutch("nl", 0));
}

static lang_choice_t asked(const char *query)
{
    return lang_from_query(query, strlen(query));
}

void test_query_names_the_language(void)
{
    TEST_ASSERT_EQUAL(LANG_DUTCH, asked("lang=nl"));
    TEST_ASSERT_EQUAL(LANG_ENGLISH, asked("lang=en"));
    TEST_ASSERT_EQUAL(LANG_DUTCH, asked("a=1&lang=nl"));
    TEST_ASSERT_EQUAL(LANG_ENGLISH, asked("lang=en&b=2"));
}

void test_query_without_a_known_language(void)
{
    TEST_ASSERT_EQUAL(LANG_UNSET, lang_from_query(NULL, 0));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked(""));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("lang="));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("lang=fr"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("lang=nlx"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("lang=NL"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("xlang=nl"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("language=nl"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("nl"));
}

void test_query_first_one_counts(void)
{
    TEST_ASSERT_EQUAL(LANG_DUTCH, asked("lang=nl&lang=en"));
    TEST_ASSERT_EQUAL(LANG_UNSET, asked("lang=fr&lang=nl"));
}

void test_query_length_is_respected(void)
{
    TEST_ASSERT_EQUAL(LANG_UNSET, lang_from_query("lang=nl", 6));
    TEST_ASSERT_EQUAL(LANG_DUTCH, lang_from_query("lang=nl&lang=en", 7));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_header_is_english);
    RUN_TEST(test_dutch_phone);
    RUN_TEST(test_english_phone);
    RUN_TEST(test_other_first_language_then_the_higher_of_the_two);
    RUN_TEST(test_equal_weight_first_listed_wins);
    RUN_TEST(test_case_and_spaces);
    RUN_TEST(test_only_the_language_nl_counts);
    RUN_TEST(test_weight_zero_means_not_wanted);
    RUN_TEST(test_bad_weights_are_ignored);
    RUN_TEST(test_cut_off_header);
    RUN_TEST(test_length_is_respected);
    RUN_TEST(test_query_names_the_language);
    RUN_TEST(test_query_without_a_known_language);
    RUN_TEST(test_query_first_one_counts);
    RUN_TEST(test_query_length_is_respected);
    return UNITY_END();
}
