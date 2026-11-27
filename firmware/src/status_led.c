#include "status_led.h"

#include <stdatomic.h>

#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "setup_button.h"
#include "wifi.h"

#define SLOW_BLINK_TICKS   (250 / UI_TICK_MS) /* 2 Hz: looking for WiFi */
#define DOUBLE_BLINK_TICKS (125 / UI_TICK_MS) /* setup: on-off-on, then a pause */
#define DOUBLE_BLINK_SLOTS 8                  /* one double blink per second */
#define FLICKER_TICKS      1
_Static_assert(DOUBLE_BLINK_TICKS >= 1, "UI_TICK_MS too long for the double blink");

static atomic_int s_flicker_ticks;
static atomic_bool s_setup;

static void led_write(bool on)
{
    gpio_set_level(LED_GPIO, on ? 0 : 1); /* active low */
}

static void tick_cb(void *arg)
{
    (void)arg;
    static unsigned ticks;

    ticks++;
    if (atomic_load(&s_setup)) {
        unsigned slot = (ticks / DOUBLE_BLINK_TICKS) % DOUBLE_BLINK_SLOTS;
        led_write(slot == 0 || slot == 2);
    } else if (!wifi_is_connected()) {
        led_write((ticks / SLOW_BLINK_TICKS) & 1);
    } else if (atomic_load(&s_flicker_ticks) > 0) {
        atomic_fetch_sub(&s_flicker_ticks, 1);
        led_write(false);
    } else {
        led_write(true);
    }
    setup_button_poll();
}

void status_led_init(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led_write(false);

    esp_timer_handle_t timer;
    const esp_timer_create_args_t timer_args = { .callback = tick_cb, .name = "ui_tick" };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, UI_TICK_MS * 1000));
}

void status_led_flicker(void)
{
    atomic_store(&s_flicker_ticks, FLICKER_TICKS);
}

void status_led_show_setup(void)
{
    atomic_store(&s_setup, true);
}
