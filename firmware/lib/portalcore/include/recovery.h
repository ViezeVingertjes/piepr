#pragma once

/*
 * When normal mode gives up on the saved network and offers the setup hotspot
 * again. Only before the first connection since boot: a network that was
 * reached once is retried (router restart, short outage). Plain C, unit-tested.
 */

#include <stdbool.h>

#include "setup_types.h"

#define RECOVERY_AFTER_S    300 /* routers need 1-3 min after a power cut */
#define RECOVERY_AUTH_FAILS 3   /* consecutive authentication failures */

setup_reason_t recovery_check(bool ever_connected, unsigned auth_failures, unsigned uptime_s);
