#include "captive_dns.h"

#include <string.h>

#define HEADER_LEN       12
#define MAX_LABEL_LEN    63
#define QTYPE_QCLASS_LEN 4
#define ANSWER_LEN       16 /* name pointer 2 + type 2 + class 2 + TTL 4 + rdlength 2 + IPv4 4 */

#define FLAG_QR        0x80 /* byte 2 */
#define FLAG_AA        0x04 /* byte 2 */
#define FLAG_RD        0x01 /* byte 2 */
#define OPCODE_MASK    0x78 /* byte 2 */
#define LABEL_PTR_BITS 0xC0

#define TYPE_A   1
#define CLASS_IN 1

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

static void write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* Length of the question section (name + QTYPE + QCLASS) starting at offset
 * HEADER_LEN, or 0 when it is malformed or runs past `len`. */
static size_t question_len(const uint8_t *query, size_t len)
{
    size_t pos = HEADER_LEN;
    for (;;) {
        if (pos >= len) {
            return 0; /* name not terminated */
        }
        uint8_t label = query[pos];
        if (label == 0) {
            pos++;
            break;
        }
        if ((label & LABEL_PTR_BITS) != 0 || label > MAX_LABEL_LEN) {
            return 0; /* compression pointer or reserved bits: not in a question */
        }
        pos += 1u + label;
    }
    if (pos + QTYPE_QCLASS_LEN > len) {
        return 0;
    }
    return pos + QTYPE_QCLASS_LEN - HEADER_LEN;
}

size_t captive_dns_reply(const uint8_t *query, size_t len, const uint8_t ipv4[4], uint8_t *out,
                         size_t out_size)
{
    if (len < HEADER_LEN) {
        return 0;
    }
    if ((query[2] & FLAG_QR) || (query[2] & OPCODE_MASK) || read_u16(query + 4) != 1) {
        return 0; /* response, non-standard opcode, or not exactly one question */
    }
    size_t qlen = question_len(query, len);
    if (qlen == 0) {
        return 0;
    }
    const uint8_t *qtail = query + HEADER_LEN + qlen - QTYPE_QCLASS_LEN;
    int answer = read_u16(qtail) == TYPE_A && read_u16(qtail + 2) == CLASS_IN;

    size_t reply_len = HEADER_LEN + qlen + (answer ? ANSWER_LEN : 0);
    if (reply_len > out_size) {
        return 0;
    }

    /* Header: same ID, QR + AA (+ RD as requested), RCODE 0. */
    out[0] = query[0];
    out[1] = query[1];
    out[2] = (uint8_t)(FLAG_QR | FLAG_AA | (query[2] & FLAG_RD));
    out[3] = 0;
    write_u16(out + 4, 1);              /* QDCOUNT */
    write_u16(out + 6, answer ? 1 : 0); /* ANCOUNT */
    write_u16(out + 8, 0);              /* NSCOUNT */
    write_u16(out + 10, 0);             /* ARCOUNT */
    memcpy(out + HEADER_LEN, query + HEADER_LEN, qlen);

    if (answer) {
        uint8_t *a = out + HEADER_LEN + qlen;
        write_u16(a, 0xC000 | HEADER_LEN); /* pointer to the question's name */
        write_u16(a + 2, TYPE_A);
        write_u16(a + 4, CLASS_IN);
        write_u16(a + 6, 0); /* TTL, high 16 bits */
        write_u16(a + 8, CAPTIVE_DNS_TTL_S);
        write_u16(a + 10, 4); /* RDLENGTH */
        memcpy(a + 12, ipv4, 4);
    }
    return reply_len;
}
