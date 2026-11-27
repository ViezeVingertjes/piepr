#include <unity.h>

#include "channel_pick.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_aps_picks_middle(void)
{
    TEST_ASSERT_EQUAL_UINT8(6, channel_pick_quietest(NULL, 0));
}

void test_result_always_in_2_to_11(void)
{
    /* Everything from 2 to 11 is loaded; 1 and 12/13 are empty but banned. */
    channel_pick_ap_t aps[10];
    for (int i = 0; i < 10; i++) {
        aps[i] = (channel_pick_ap_t){ (uint8_t)(i + 2), -40 };
    }
    uint8_t ch = channel_pick_quietest(aps, 10);
    TEST_ASSERT_TRUE(ch >= CHANNEL_PICK_FIRST && ch <= CHANNEL_PICK_LAST);
}

void test_busy_low_band_picks_first_free_above(void)
{
    /* APs on 1 and 3 overlap channels up to 7; 8 is the first without overlap. */
    const channel_pick_ap_t aps[] = { { 1, -40 }, { 3, -40 } };
    TEST_ASSERT_EQUAL_UINT8(8, channel_pick_quietest(aps, 2));
}

void test_busy_high_band_picks_low(void)
{
    const channel_pick_ap_t aps[] = { { 11, -40 }, { 8, -40 } };
    TEST_ASSERT_EQUAL_UINT8(2, channel_pick_quietest(aps, 2));
}

void test_weak_aps_weigh_less(void)
{
    /* Three very weak APs on 6 (load 15) versus strong ones on 1 and 12, which
     * load 2..5 and 8..11 with 60: the weak ones around 6/7 win. */
    const channel_pick_ap_t aps[] = { { 6, -95 }, { 6, -95 }, { 6, -95 }, { 1, -40 }, { 12, -40 } };
    TEST_ASSERT_EQUAL_UINT8(6, channel_pick_quietest(aps, 5));
}

void test_rssi_below_noise_clamped(void)
{
    /* Both APs overlap 2..10; only 11 is free. Clamped, 2..10 score 1 and 11
     * wins. Unclamped, the -120 dBm AP would make 2..10 score -19 and 2 win. */
    const channel_pick_ap_t aps[] = { { 6, -120 }, { 6, -99 } };
    TEST_ASSERT_EQUAL_UINT8(11, channel_pick_quietest(aps, 2));
}

void test_far_channels_ignored(void)
{
    /* Channel 14 overlaps 10..11 only; channel 2 overlaps up to 6. */
    const channel_pick_ap_t aps[] = { { 14, -30 }, { 2, -60 } };
    TEST_ASSERT_EQUAL_UINT8(7, channel_pick_quietest(aps, 2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_aps_picks_middle);
    RUN_TEST(test_result_always_in_2_to_11);
    RUN_TEST(test_busy_low_band_picks_first_free_above);
    RUN_TEST(test_busy_high_band_picks_low);
    RUN_TEST(test_weak_aps_weigh_less);
    RUN_TEST(test_rssi_below_noise_clamped);
    RUN_TEST(test_far_channels_ignored);
    return UNITY_END();
}
