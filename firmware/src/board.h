#pragma once

/* The board this firmware is built for. Each [env:<board>] in platformio.ini
 * defines one PIEPR_BOARD_* flag; its header in boards/ gives the pins:
 *
 *   BOARD_NAME               in the start-up log and the release manifest.json
 *   BOARD_P1_RX_GPIO         P1 data (meter pin 5), with the 2.2k pull-up to 3V3
 *   BOARD_LED_GPIO           status LED
 *   BOARD_LED_ACTIVE_LOW     1 when the LED lights with the pin low
 *   BOARD_SETUP_BUTTON_GPIO  optional: active-low button for WiFi setup (BOOT);
 *                            leave it out on a board without one
 *
 * Adding a board: see "Boards" in firmware/README.md. */

#if defined(PIEPR_BOARD_SUPERMINI)
#include "boards/supermini.h"
#elif defined(PIEPR_BOARD_XIAO_S3)
#include "boards/xiao_s3.h"
#else
#error "no board selected: build one of the board envs in platformio.ini (pio run -e <board>)"
#endif

#if !defined(BOARD_NAME) || !defined(BOARD_P1_RX_GPIO) || !defined(BOARD_LED_GPIO) ||              \
    !defined(BOARD_LED_ACTIVE_LOW)
#error "the board header is missing a setting (see board.h)"
#endif
