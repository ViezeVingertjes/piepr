#include "p1_reader.h"

#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "p1_framer.h"
#include "p1_ident.h"
#include "p1_parse.h"
#include "status_led.h"
#include "tcp_server.h"

/* Results are counted and logged once per STATS_PERIOD_S, never per event: an
 * unplugged cable gives a continuous break condition, a noisy line many bad frames.
 *
 * Each valid telegram is sent to the readers first and only then parsed for the
 * status page. What other tasks may ask for (p1_reader_status) is one record
 * behind s_lock, held just long enough to change or copy it. The reader task is
 * its only writer, so it reads its own record without the lock. */

#define P1_UART_NUM            UART_NUM_1
#define UART_RX_BUF_SIZE       2048 /* drained continuously; ~180 ms of slack */
#define UART_EVENT_QUEUE_LEN   20
#define UART_RX_FULL_THRESHOLD 64 /* of 128 FIFO bytes (default 120): ~5 ms headroom */
#define READ_CHUNK_SIZE        256
#define EVENT_TIMEOUT_MS       1000
_Static_assert(EVENT_TIMEOUT_MS < CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000, "must feed the watchdog");
#define STATS_PERIOD_S 60

static const char *TAG = LOG_TAG_P1;

static QueueHandle_t s_uart_queue;
static p1_framer_t s_framer; /* 4 KB: static, not on the task stack */

static SemaphoreHandle_t s_lock;
static p1_status_t s_status;  /* under s_lock; the ages are worked out when asked */
static int64_t s_last_ok_us;  /* under s_lock: the last valid telegram, or start-up */
static int64_t s_rejected_us; /* under s_lock: the last rejected one */
static p1_counts_t s_logged;  /* the counts at the last log line: it shows the difference */
static bool s_receiving;      /* cleared when a minute passes without a valid telegram */
static p1_values_t s_parsed;  /* reader task only: parsed here, outside the lock */

static void keep_telegram(const uint8_t *telegram, size_t len)
{
    char meter[P1_METER_NAME_MAX + 1];
    p1_ident(telegram, len, meter, sizeof(meter));
    p1_parse(telegram, len, &s_parsed);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.counts.ok++;
    s_status.seen = true;
    memcpy(s_status.meter, meter, sizeof(s_status.meter));
    s_status.values = s_parsed;
    s_last_ok_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);
}

/* Adds one to `*count`, a member of s_status.counts. */
static void count(uint32_t *count, bool rejected)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    (*count)++;
    if (rejected) {
        s_status.rejected = true;
        s_rejected_us = esp_timer_get_time();
    }
    xSemaphoreGive(s_lock);
}

static void feed_bytes(const uint8_t *data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        switch (p1_framer_push(&s_framer, data[i])) {
        case P1_FRAME_OK: {
            size_t len;
            const uint8_t *t = p1_framer_telegram(&s_framer, &len);
            tcp_server_broadcast(t, len);
            status_led_flicker();
            keep_telegram(t, len);
            if (!s_receiving) {
                s_receiving = true;
                ESP_LOGI(TAG, "telegrams arriving from \"%s\" (%u bytes each)", s_status.meter,
                         (unsigned)len);
            }
            break;
        }
        case P1_FRAME_CRC_ERROR:
            count(&s_status.counts.crc_errors, true);
            break;
        case P1_FRAME_OVERFLOW:
            count(&s_status.counts.overflows, true);
            break;
        case P1_FRAME_NONE:
        default:
            break;
        }
    }
}

static void handle_event(const uart_event_t *ev)
{
    static uint8_t chunk[READ_CHUNK_SIZE];

    switch (ev->type) {
    case UART_DATA: {
        /* Drain everything buffered, not just ev->size: if an event was ever
         * lost (queue full), its bytes would otherwise lag behind forever. */
        int n;
        while ((n = uart_read_bytes(P1_UART_NUM, chunk, sizeof(chunk), 0)) > 0) {
            feed_bytes(chunk, (size_t)n);
        }
        break;
    }
    case UART_FIFO_OVF:
    case UART_BUFFER_FULL:
        xQueueReset(s_uart_queue); /* before the flush: no event may outlive it */
        uart_flush_input(P1_UART_NUM);
        p1_framer_init(&s_framer);
        count(&s_status.counts.uart_errors, false);
        break;
    case UART_BREAK:
        p1_framer_init(&s_framer);
        count(&s_status.counts.uart_errors, false);
        break;
    default:
        break;
    }
}

/* One line per minute with the counts; a silent minute is a warning, and the
 * next valid telegram is announced again. */
static void log_stats(int64_t now)
{
    const p1_counts_t *counts = &s_status.counts;
    int64_t idle_s = (now - s_last_ok_us) / US_PER_S;
    bool stale = idle_s >= STATS_PERIOD_S;
    ESP_LOG_LEVEL(
        stale ? ESP_LOG_WARN : ESP_LOG_INFO, TAG,
        "last %ds: ok %lu, crc %lu, overflow %lu, uart errors %lu; last valid telegram %llds ago%s",
        STATS_PERIOD_S, (unsigned long)(counts->ok - s_logged.ok),
        (unsigned long)(counts->crc_errors - s_logged.crc_errors),
        (unsigned long)(counts->overflows - s_logged.overflows),
        (unsigned long)(counts->uart_errors - s_logged.uart_errors), idle_s,
        stale ? " (check wiring / pull-up / pin 2)" : "");
    if (stale) {
        s_receiving = false;
    }
    s_logged = *counts;
}

static void reader_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    int64_t next_stats_us = esp_timer_get_time() + STATS_PERIOD_S * US_PER_S;

    for (;;) {
        esp_task_wdt_reset();

        uart_event_t ev;
        if (xQueueReceive(s_uart_queue, &ev, pdMS_TO_TICKS(EVENT_TIMEOUT_MS)) == pdTRUE) {
            handle_event(&ev);
        }

        int64_t now = esp_timer_get_time();
        if (now >= next_stats_us) {
            next_stats_us = now + STATS_PERIOD_S * US_PER_S;
            log_stats(now);
        }
    }
}

void p1_reader_start(void)
{
    p1_framer_init(&s_framer);
    s_lock = xSemaphoreCreateMutex();
    configASSERT(s_lock);
    s_last_ok_us = esp_timer_get_time(); /* "ago" counts from start until one arrives */

    const uart_config_t cfg = {
        .baud_rate = P1_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(P1_UART_NUM, UART_RX_BUF_SIZE, 0, UART_EVENT_QUEUE_LEN,
                                        &s_uart_queue, ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(uart_param_config(P1_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(P1_UART_NUM, UART_PIN_NO_CHANGE, BOARD_P1_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_line_inverse(P1_UART_NUM, UART_SIGNAL_RXD_INV));
    /* Headroom when interrupts are briefly delayed. */
    ESP_ERROR_CHECK(uart_set_rx_full_threshold(P1_UART_NUM, UART_RX_FULL_THRESHOLD));
    gpio_pullup_en(BOARD_P1_RX_GPIO); /* in parallel with the external 2.2k */

    BaseType_t ok = xTaskCreate(reader_task, "p1_reader", P1_READER_TASK_STACK, NULL,
                                P1_READER_TASK_PRIO, NULL);
    configASSERT(ok == pdPASS);
    ESP_LOGI(TAG, "reading P1 on GPIO%d, %d baud, inverted", BOARD_P1_RX_GPIO, P1_BAUD);
}

static uint32_t ms_since(int64_t then_us, int64_t now_us)
{
    int64_t ms = (now_us - then_us) / 1000;
    return ms > UINT32_MAX ? UINT32_MAX : (uint32_t)ms;
}

void p1_reader_status(p1_status_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    int64_t last_ok_us = s_last_ok_us, rejected_us = s_rejected_us;
    xSemaphoreGive(s_lock);

    int64_t now_us = esp_timer_get_time();
    out->age_ms = ms_since(last_ok_us, now_us);
    out->rejected_age_ms = ms_since(rejected_us, now_us);
}
