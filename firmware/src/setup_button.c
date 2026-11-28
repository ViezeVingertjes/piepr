#include "setup_button.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = LOG_TAG_BUTTON;

#ifdef BOARD_SETUP_BUTTON_GPIO

#include <stdbool.h>

#include "driver/gpio.h"
#include "setup_portal.h"

#define HOLD_TICKS (SETUP_HOLD_S * 1000 / UI_TICK_MS)

static bool s_enabled;
static bool s_armed; /* released once since boot, so a stuck button can't loop setup */
static unsigned s_held_ticks;

void setup_button_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_SETUP_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    s_enabled = true;
    ESP_LOGI(TAG, "hold BOOT for %d s to start WiFi setup", SETUP_HOLD_S);
}

void setup_button_poll(void)
{
    if (!s_enabled) {
        return;
    }
    if (gpio_get_level(BOARD_SETUP_BUTTON_GPIO) != 0) {
        s_armed = true;
        s_held_ticks = 0;
    } else if (s_armed && ++s_held_ticks == HOLD_TICKS) {
        setup_portal_request(SETUP_REASON_NONE, NULL);
    }
}

#else /* a board without a setup button: setup comes only from recovery */

void setup_button_init(void)
{
    ESP_LOGI(TAG, "no setup button on this board");
}

void setup_button_poll(void) {}

#endif
