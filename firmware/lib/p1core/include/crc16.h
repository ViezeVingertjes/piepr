#pragma once

#include <stddef.h>
#include <stdint.h>

/* CRC-16/ARC (poly 0xA001 reflected, init 0x0000, no final XOR), as used by
 * DSMR P1 telegrams over the bytes '/' up to and including '!'. */
uint16_t crc16_arc(const uint8_t *data, size_t len);
