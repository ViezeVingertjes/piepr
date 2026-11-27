#include "channel_pick.h"

#define NOISE_REFERENCE_DBM 100 /* weight = rssi + 100, clamped at 0 */
#define OVERLAP_CHANNELS    4   /* 20 MHz channels closer than 5 apart overlap */

uint8_t channel_pick_quietest(const channel_pick_ap_t *aps, size_t count)
{
    if (count == 0) {
        return CHANNEL_PICK_DEFAULT;
    }
    uint8_t best = CHANNEL_PICK_FIRST;
    long best_load = -1;
    for (int candidate = CHANNEL_PICK_FIRST; candidate <= CHANNEL_PICK_LAST; candidate++) {
        long load = 0;
        for (size_t i = 0; i < count; i++) {
            int distance = aps[i].channel - candidate;
            if (distance < -OVERLAP_CHANNELS || distance > OVERLAP_CHANNELS) {
                continue;
            }
            int weight = aps[i].rssi + NOISE_REFERENCE_DBM;
            load += weight > 0 ? weight : 0;
        }
        if (best_load < 0 || load < best_load) { /* strict: ties keep the lower channel */
            best = (uint8_t)candidate;
            best_load = load;
        }
    }
    return best;
}
