#pragma once

#include <stdbool.h>

#include "setup_types.h"

/*
 * WiFi setup portal. Requesting it (BOOT held SETUP_HOLD_S, or no WiFi known)
 * scans for the quietest channel and restarts into a separate setup boot that
 * runs only the hotspot SETUP_AP_SSID, the web form and a captive DNS. Saving
 * restarts into normal mode.
 */

/* Normal mode (station running): scan, then restart into the setup boot, where
 * the page shows `reason` for `ssid` (copied; may be NULL). Only the first
 * request counts. */
void setup_portal_request(setup_reason_t reason, const char *ssid);

/* At boot: true (once) when the previous run requested setup. */
bool setup_portal_requested(void);

/* Setup boot; never returns. has_wifi: return to normal mode SETUP_TIMEOUT_S
 * (RECOVERY_SETUP_WINDOW_S when entered by recovery) after the last page activity;
 * otherwise wait for a save. */
void setup_portal_run(bool has_wifi) __attribute__((noreturn));
