#include "recovery.h"

setup_reason_t recovery_check(bool ever_connected, unsigned auth_failures, unsigned uptime_s)
{
    if (ever_connected) {
        return SETUP_REASON_NONE;
    }
    if (auth_failures >= RECOVERY_AUTH_FAILS) {
        return SETUP_REASON_AUTH;
    }
    if (uptime_s >= RECOVERY_AFTER_S) {
        return SETUP_REASON_NO_CONNECT;
    }
    return SETUP_REASON_NONE;
}
