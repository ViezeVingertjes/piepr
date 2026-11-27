#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "crc16.h"
#include "p1_framer.h"

static p1_framer_t f;

static const char *BODY = "/ISK5\\2M550T-1012\r\n"
                          "\r\n"
                          "1-3:0.2.8(50)\r\n"
                          "1-0:1.7.0(00.452*kW)\r\n";

void setUp(void)
{
    p1_framer_init(&f);
}
void tearDown(void) {}

/* Builds body + "!" + CRC (4 hex digits) + "\r\n" into out; returns length.
 * body_len is explicit so bodies may contain NUL bytes. */
static size_t make_telegram_fmt(const char *body, size_t body_len, const char *crc_fmt, char *out)
{
    size_t n = body_len;
    memcpy(out, body, n);
    out[n++] = '!';
    uint16_t crc = crc16_arc((const uint8_t *)out, n);
    n += (size_t)sprintf(out + n, crc_fmt, crc);
    out[n++] = '\r';
    out[n++] = '\n';
    out[n] = '\0';
    return n;
}

static size_t make_telegram(const char *body, char *out)
{
    return make_telegram_fmt(body, strlen(body), "%04X", out);
}

/* Pushes n bytes; returns the last non-NONE result (NONE if none) and
 * fails the test if more than one non-NONE result occurs. */
static p1_frame_result_t push_all(const char *s, size_t n)
{
    p1_frame_result_t last = P1_FRAME_NONE;
    int events = 0;
    for (size_t i = 0; i < n; i++) {
        p1_frame_result_t r = p1_framer_push(&f, (uint8_t)s[i]);
        if (r != P1_FRAME_NONE) {
            last = r;
            events++;
        }
    }
    TEST_ASSERT_LESS_OR_EQUAL_INT(1, events);
    return last;
}

static void assert_telegram_equals(const char *expected, size_t n)
{
    size_t len = 0;
    const uint8_t *t = p1_framer_telegram(&f, &len);
    TEST_ASSERT_EQUAL_size_t(n, len);
    TEST_ASSERT_EQUAL_MEMORY(expected, t, n);
}

void test_valid_telegram_ok(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_crc_mismatch_is_error(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    t[30] ^= 0x01; /* corrupt one body byte after the CRC was computed */
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(t, n));
}

void test_crc_lowercase_hex_accepted(void)
{
    char t[256];
    /* Pick a body whose CRC contains a letter so case actually matters. */
    const char *body = "/ABC5\\2test\r\n\r\n1-0:1.8.1(001234.567*kWh)\r\n";
    size_t n = make_telegram_fmt(body, strlen(body), "%04x", t);
    const char *crc = strchr(t, '!') + 1;
    int has_letter = 0;
    for (int i = 0; i < 4; i++) {
        if (crc[i] >= 'a' && crc[i] <= 'f') {
            has_letter = 1;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(has_letter, "test body must produce a CRC with a-f");
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_crc_non_hex_is_error(void)
{
    char t[256];
    size_t n = strlen(BODY);
    memcpy(t, BODY, n);
    memcpy(t + n, "!12G4\r\n", 7);
    n += 7;
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(t, n));
}

/* Random bytes, with '/', '!', CR/LF and hex digits injected often so the
 * framer reaches every state; a 16-bit CRC match by chance is ~1/65536 per
 * candidate frame, so a fixed seed keeps this deterministic. */
void test_garbage_never_yields_ok(void)
{
    static const uint8_t special[] = { '/', '!', '\r', '\n', 'A', '0' };
    uint32_t seed = 12345;
    int reached_crc_state = 0;
    for (int i = 0; i < 200000; i++) {
        seed = seed * 1103515245u + 12345u;
        uint8_t b = (uint8_t)(seed >> 16);
        if ((seed >> 8) % 4 == 0) {
            b = special[(seed >> 24) % sizeof(special)];
        }
        p1_frame_result_t r = p1_framer_push(&f, b);
        TEST_ASSERT_NOT_EQUAL(P1_FRAME_OK, r);
        if (r == P1_FRAME_CRC_ERROR) {
            reached_crc_state = 1;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(reached_crc_state, "garbage never reached the CRC check");
}

void test_leading_partial_telegram_ignored(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    /* Second half of a telegram (contains no '/'), as when starting mid-stream. */
    const char *half = t + n / 2;
    TEST_ASSERT_NULL(memchr(half, '/', n - n / 2));
    TEST_ASSERT_EQUAL(P1_FRAME_NONE, push_all(half, n - n / 2));
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_slash_mid_body_resyncs(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    const char *junk = "/garbage\r\n1-0:1.8";
    TEST_ASSERT_EQUAL(P1_FRAME_NONE, push_all(junk, strlen(junk)));
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_two_telegrams_back_to_back(void)
{
    char a[256], b[256];
    size_t na = make_telegram(BODY, a);
    size_t nb = make_telegram("/XYZ5\\other\r\n\r\n0-0:96.14.0(0002)\r\n", b);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(a, na));
    assert_telegram_equals(a, na);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(b, nb));
    assert_telegram_equals(b, nb);
}

void test_overflow_then_recovers(void)
{
    static char big[P1_MAX_TELEGRAM + 101];
    big[0] = '/';
    memset(big + 1, 'a', P1_MAX_TELEGRAM + 100);
    TEST_ASSERT_EQUAL(P1_FRAME_OVERFLOW, push_all(big, P1_MAX_TELEGRAM + 101));

    char t[256];
    size_t n = make_telegram(BODY, t);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_max_size_telegram_ok(void)
{
    /* Total = body + "!" + 4 hex + "\r\n" = P1_MAX_TELEGRAM exactly. */
    static char body[P1_MAX_TELEGRAM];
    static char t[P1_MAX_TELEGRAM + 8];
    size_t body_len = P1_MAX_TELEGRAM - 7;
    body[0] = '/';
    memset(body + 1, 'b', body_len - 1);
    body[body_len] = '\0';
    size_t n = make_telegram(body, t);
    TEST_ASSERT_EQUAL_size_t(P1_MAX_TELEGRAM, n);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_missing_newline_after_crc_recovers(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    /* Valid "!XXXX" but the line ending never arrives, noise instead. */
    char broken[300];
    size_t bang = (size_t)(strchr(t, '!') - t);
    memcpy(broken, t, bang + 5);
    memset(broken + bang + 5, 'z', 50);
    size_t bn = bang + 5 + 50;
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(broken, bn));

    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_nul_and_high_bytes_in_body_no_crash(void)
{
    char body[64] = "/ISK5\\2X\r\n\r\n1-0:1.7.0(";
    size_t bl = strlen(body);
    body[bl++] = '\0';
    body[bl++] = (char)0xFF;
    memcpy(body + bl, ")\r\n", 3);
    bl += 3;

    char t[128];
    size_t n = make_telegram_fmt(body, bl, "%04X", t);

    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

/* dsmr_parser's published TELEGRAM_V5 sample (ISKRA MT382, 3-phase, 2 M-Bus
 * channels) with its literal CRC: pins the algorithm AND the byte range
 * independently of crc16_arc(). */
static const char V5[] = "/ISk5\\2MT382-1000\r\n"
                         "\r\n"
                         "1-3:0.2.8(50)\r\n"
                         "0-0:1.0.0(170102192002W)\r\n"
                         "0-0:96.1.1(4B384547303034303436333935353037)\r\n"
                         "1-0:1.8.1(000004.426*kWh)\r\n"
                         "1-0:1.8.2(000002.399*kWh)\r\n"
                         "1-0:2.8.1(000002.444*kWh)\r\n"
                         "1-0:2.8.2(000000.000*kWh)\r\n"
                         "0-0:96.14.0(0002)\r\n"
                         "1-0:1.7.0(00.244*kW)\r\n"
                         "1-0:2.7.0(00.000*kW)\r\n"
                         "0-0:96.7.21(00013)\r\n"
                         "0-0:96.7.9(00000)\r\n"
                         "1-0:99.97.0(0)(0-0:96.7.19)\r\n"
                         "1-0:32.32.0(00000)\r\n"
                         "1-0:52.32.0(00000)\r\n"
                         "1-0:72.32.0(00000)\r\n"
                         "1-0:32.36.0(00000)\r\n"
                         "1-0:52.36.0(00000)\r\n"
                         "1-0:72.36.0(00000)\r\n"
                         "0-0:96.13.0()\r\n"
                         "1-0:32.7.0(0230.0*V)\r\n"
                         "1-0:52.7.0(0230.0*V)\r\n"
                         "1-0:72.7.0(0229.0*V)\r\n"
                         "1-0:31.7.0(0.48*A)\r\n"
                         "1-0:51.7.0(0.44*A)\r\n"
                         "1-0:71.7.0(0.86*A)\r\n"
                         "1-0:21.7.0(00.070*kW)\r\n"
                         "1-0:41.7.0(00.032*kW)\r\n"
                         "1-0:61.7.0(00.142*kW)\r\n"
                         "1-0:22.7.0(00.000*kW)\r\n"
                         "1-0:42.7.0(00.000*kW)\r\n"
                         "1-0:62.7.0(00.000*kW)\r\n"
                         "0-1:24.1.0(003)\r\n"
                         "0-1:96.1.0(3232323241424344313233343536373839)\r\n"
                         "0-1:24.2.1(170102161005W)(00000.107*m3)\r\n"
                         "0-2:24.1.0(003)\r\n"
                         "0-2:96.1.0()\r\n"
                         "!6EEE\r\n";

void test_real_dsmr5_telegram_published_crc(void)
{
    size_t n = strlen(V5);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(V5, n));
    assert_telegram_equals(V5, n);
}

/* Operator text messages (0-0:96.13.0) can be up to 2048 hex chars. */
void test_telegram_over_2048_bytes_accepted(void)
{
    static char body[3001];
    static char t[3016];
    body[0] = '/';
    memset(body + 1, 'c', 2999);
    body[3000] = '\0';
    size_t n = make_telegram(body, t);
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

void test_one_byte_over_max_is_overflow(void)
{
    static char body[P1_MAX_TELEGRAM];
    static char t[P1_MAX_TELEGRAM + 16];
    size_t body_len = P1_MAX_TELEGRAM - 6; /* total = MAX + 1 */
    body[0] = '/';
    memset(body + 1, 'd', body_len - 1);
    body[body_len] = '\0';
    size_t n = make_telegram(body, t);
    TEST_ASSERT_EQUAL_size_t(P1_MAX_TELEGRAM + 1, n);
    TEST_ASSERT_EQUAL(P1_FRAME_OVERFLOW, push_all(t, n));
}

void test_lf_only_line_ending_ok(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    t[n - 2] = '\n'; /* "\r\n" -> "\n" */
    n--;
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
}

void test_double_cr_is_crc_error(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    memcpy(t + n - 1, "\r\n", 2); /* "...XXXX\r\r\n" */
    n++;
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(t, n));
}

void test_junk_after_cr_is_crc_error(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    t[n - 1] = 'Z'; /* "...XXXX\rZ" */
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(t, n));
}

void test_short_crc_is_crc_error(void)
{
    char t[256];
    size_t n = strlen(BODY);
    memcpy(t, BODY, n);
    memcpy(t + n, "!12\r\n", 5);
    TEST_ASSERT_EQUAL(P1_FRAME_CRC_ERROR, push_all(t, n + 5));
}

void test_slash_in_crc_resyncs(void)
{
    char t[256];
    size_t n = make_telegram(BODY, t);
    const char *junk = "/x\r\n!12";
    TEST_ASSERT_EQUAL(P1_FRAME_NONE, push_all(junk, strlen(junk)));
    TEST_ASSERT_EQUAL(P1_FRAME_OK, push_all(t, n));
    assert_telegram_equals(t, n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_telegram_ok);
    RUN_TEST(test_crc_mismatch_is_error);
    RUN_TEST(test_crc_lowercase_hex_accepted);
    RUN_TEST(test_crc_non_hex_is_error);
    RUN_TEST(test_garbage_never_yields_ok);
    RUN_TEST(test_leading_partial_telegram_ignored);
    RUN_TEST(test_slash_mid_body_resyncs);
    RUN_TEST(test_two_telegrams_back_to_back);
    RUN_TEST(test_overflow_then_recovers);
    RUN_TEST(test_max_size_telegram_ok);
    RUN_TEST(test_missing_newline_after_crc_recovers);
    RUN_TEST(test_nul_and_high_bytes_in_body_no_crash);
    RUN_TEST(test_real_dsmr5_telegram_published_crc);
    RUN_TEST(test_telegram_over_2048_bytes_accepted);
    RUN_TEST(test_one_byte_over_max_is_overflow);
    RUN_TEST(test_lf_only_line_ending_ok);
    RUN_TEST(test_double_cr_is_crc_error);
    RUN_TEST(test_junk_after_cr_is_crc_error);
    RUN_TEST(test_short_crc_is_crc_error);
    RUN_TEST(test_slash_in_crc_resyncs);
    return UNITY_END();
}
