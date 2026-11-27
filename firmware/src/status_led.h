#pragma once

/* LED: slow blink = no WiFi, double blink = setup, on = connected, flicker =
 * telegram. Its UI tick also polls the BOOT button. */
void status_led_init(void);
void status_led_flicker(void);
void status_led_show_setup(void);
