/* libFuzzer harness: arbitrary UDP payloads into captive_dns_reply. Run: fuzz/run.sh captive_dns */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "captive_dns.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const uint8_t ip[4] = { 192, 168, 4, 1 };
    uint8_t out[CAPTIVE_DNS_MAX_PACKET];
    size_t n = captive_dns_reply(data, size, ip, out, sizeof(out));
    assert(n <= sizeof(out));
    if (n > 0) {
        assert(out[0] == data[0] && out[1] == data[1]); /* same query ID */
        assert(out[2] & 0x80);                          /* it is a response */
    }
    return 0;
}
