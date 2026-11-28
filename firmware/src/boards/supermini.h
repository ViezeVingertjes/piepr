#pragma once

/* ESP32-C3 SuperMini (not "Plus"). Right-hand row, USB-C up:
 * 5V, G, 3.3, 4, 3, 2, 1, 0. Avoid GPIO2, 8 and 9 for P1: strapping pins. */

#include "sdkconfig.h"
#if !CONFIG_IDF_TARGET_ESP32C3
#error "the ESP32-C3 SuperMini needs an esp32c3 build"
#endif

#define BOARD_NAME              "ESP32-C3 SuperMini"
#define BOARD_P1_RX_GPIO        4 /* pad 4, next to 3.3 */
#define BOARD_LED_GPIO          8
#define BOARD_LED_ACTIVE_LOW    1
#define BOARD_SETUP_BUTTON_GPIO 9 /* BOOT */
