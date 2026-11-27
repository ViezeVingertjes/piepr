#include <string.h>
#include <unity.h>

#include "captive_dns.h"

static const uint8_t IP[4] = { 192, 168, 4, 1 };
static uint8_t out[CAPTIVE_DNS_MAX_PACKET];

void setUp(void)
{
    memset(out, 0xEE, sizeof(out));
}
void tearDown(void) {}

/* Standard query, ID 0x1234, flags 0x0100 (RD), 1 question: example.com, type/class given. */
static size_t make_query(uint8_t *q, uint16_t flags, uint16_t qtype)
{
    static const uint8_t header[] = { 0x12, 0x34, 0, 0, 0x00, 0x01, 0, 0, 0, 0, 0, 0 };
    static const uint8_t name[] = { 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0 };
    size_t n = 0;
    memcpy(q, header, sizeof(header));
    q[2] = (uint8_t)(flags >> 8);
    q[3] = (uint8_t)flags;
    n += sizeof(header);
    memcpy(q + n, name, sizeof(name));
    n += sizeof(name);
    q[n++] = (uint8_t)(qtype >> 8);
    q[n++] = (uint8_t)qtype;
    q[n++] = 0;
    q[n++] = 1; /* class IN */
    return n;
}

void test_a_query_answered(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0x0000, 1);
    size_t n = captive_dns_reply(q, qlen, IP, out, sizeof(out));

    TEST_ASSERT_EQUAL_size_t(qlen + 16, n);
    TEST_ASSERT_EQUAL_HEX8(0x12, out[0]); /* same ID */
    TEST_ASSERT_EQUAL_HEX8(0x34, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x84, out[2]);                       /* QR + AA */
    TEST_ASSERT_EQUAL_HEX8(0x00, out[3]);                       /* RCODE 0 */
    static const uint8_t counts[] = { 0, 1, 0, 1, 0, 0, 0, 0 }; /* QD 1, AN 1, NS 0, AR 0 */
    TEST_ASSERT_EQUAL_MEMORY(counts, out + 4, sizeof(counts));
    TEST_ASSERT_EQUAL_MEMORY(q + 12, out + 12, qlen - 12); /* question copied */
    static const uint8_t answer[] = { 0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 168, 4, 1 };
    TEST_ASSERT_EQUAL_MEMORY(answer, out + qlen, sizeof(answer));
}

void test_aaaa_query_no_answer(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0x0000, 28);
    size_t n = captive_dns_reply(q, qlen, IP, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(qlen, n);
    TEST_ASSERT_EQUAL_HEX8(0x84, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0, out[7]); /* ANCOUNT 0 */
}

void test_rd_flag_copied(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0x0100, 1);
    TEST_ASSERT_NOT_EQUAL(0, captive_dns_reply(q, qlen, IP, out, sizeof(out)));
    TEST_ASSERT_EQUAL_HEX8(0x85, out[2]); /* QR + AA + RD */
}

void test_short_packet_ignored(void)
{
    uint8_t q[64];
    make_query(q, 0, 1);
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, 11, IP, out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, 0, IP, out, sizeof(out)));
}

void test_response_packet_ignored(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0x8000, 1); /* QR set */
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, qlen, IP, out, sizeof(out)));
    qlen = make_query(q, 0x2800, 1); /* opcode 5 (UPDATE) */
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, qlen, IP, out, sizeof(out)));
}

void test_two_questions_ignored(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0, 1);
    q[5] = 2;
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, qlen, IP, out, sizeof(out)));
}

void test_unterminated_name_ignored(void)
{
    uint8_t q[64];
    make_query(q, 0, 1);
    /* Cut the packet inside the name: header + "\7example". */
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, 12 + 8, IP, out, sizeof(out)));
    /* Name terminated but QTYPE/QCLASS missing. */
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, 12 + 13 + 2, IP, out, sizeof(out)));
}

void test_compression_pointer_ignored(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0, 1);
    q[12] = 0xC0; /* pointer instead of a label length */
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, qlen, IP, out, sizeof(out)));
}

void test_label_too_long_ignored(void)
{
    uint8_t q[128] = { 0x12, 0x34, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
    size_t n = 12;
    q[n++] = 64; /* labels are at most 63 bytes */
    memset(q + n, 'a', 64);
    n += 64;
    q[n++] = 0;
    q[n++] = 0;
    q[n++] = 1;
    q[n++] = 0;
    q[n++] = 1;
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, n, IP, out, sizeof(out)));
}

void test_out_too_small_ignored(void)
{
    uint8_t q[64];
    size_t qlen = make_query(q, 0, 1);
    TEST_ASSERT_EQUAL_size_t(0, captive_dns_reply(q, qlen, IP, out, qlen + 15));
    TEST_ASSERT_EQUAL_HEX8(0xEE, out[0]); /* nothing written */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_query_answered);
    RUN_TEST(test_aaaa_query_no_answer);
    RUN_TEST(test_rd_flag_copied);
    RUN_TEST(test_short_packet_ignored);
    RUN_TEST(test_response_packet_ignored);
    RUN_TEST(test_two_questions_ignored);
    RUN_TEST(test_unterminated_name_ignored);
    RUN_TEST(test_compression_pointer_ignored);
    RUN_TEST(test_label_too_long_ignored);
    RUN_TEST(test_out_too_small_ignored);
    return UNITY_END();
}
