#pragma once

/* Seeed Studio XIAO ESP32-S3 (the plain board; see below for the Sense). Wi-Fi
 * only through the U.FL connector: fit the antenna. Power side, USB-C up:
 * 5V, GND, 3V3, D10, D9, D8, D7. Strapping pins (not for P1): GPIO0, 3, 45, 46. */

#include "sdkconfig.h"
#if !CONFIG_IDF_TARGET_ESP32S3
#error "the XIAO ESP32-S3 needs an esp32s3 build"
#endif

#define BOARD_NAME              "XIAO ESP32-S3"
#define BOARD_P1_RX_GPIO        9  /* D10, next to 3V3. The Sense's SD card also uses it */
#define BOARD_LED_GPIO          21 /* the orange user LED */
#define BOARD_LED_ACTIVE_LOW    1
#define BOARD_SETUP_BUTTON_GPIO 0 /* BOOT, the tiny button beside the USB-C socket */
