#include <unity.h>

#include "crc16.h"

void setUp(void) {}
void tearDown(void) {}

/* Standard CRC-16/ARC check value. */
void test_crc16_check_value(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xBB3D, crc16_arc((const uint8_t *)"123456789", 9));
}

void test_crc16_empty(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x0000, crc16_arc(NULL, 0));
}

void test_crc16_single_byte(void)
{
    TEST_ASSERT_EQUAL_HEX16(0x30C0, crc16_arc((const uint8_t *)"A", 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc16_check_value);
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_crc16_single_byte);
    return UNITY_END();
}
