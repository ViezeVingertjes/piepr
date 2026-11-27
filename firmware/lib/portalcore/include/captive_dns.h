#pragma once

/*
 * Captive-portal DNS: answers every A query with one fixed IPv4 address, so a
 * phone that joins the setup access point opens the setup page by itself.
 * Plain C, no ESP-IDF dependencies, unit-tested and fuzzed on the host.
 */

#include <stddef.h>
#include <stdint.h>

#define CAPTIVE_DNS_MAX_PACKET 512 /* classic DNS over UDP */
#define CAPTIVE_DNS_TTL_S      60

/* Builds the reply to `query` (`len` bytes) into `out` (`out_size` bytes).
 * Returns the reply length, or 0 when the packet must be ignored: not a
 * standard query with exactly one well-formed question, or the reply does not
 * fit. `ipv4` = 4 bytes in network order. A/IN questions get one answer; other
 * types get an empty (NOERROR) answer section. */
size_t captive_dns_reply(const uint8_t *query, size_t len, const uint8_t ipv4[4], uint8_t *out,
                         size_t out_size);
