#pragma once

/* Board and application settings; module-internal constants stay in their .c file. */

#define US_PER_S 1000000LL

/* P1 port (DSMR 4/5: 115200 8N1, inverted) */
#define P1_RX_GPIO 4
#define P1_BAUD    115200

/* Network */
#define HOSTNAME        "piepr"
#define TCP_PORT        8088
#define TCP_MAX_CLIENTS 5 /* a 6th replaces the longest-connected (see tcp_server.c) */

/* On-board LED (active low) and the tick that drives it and polls the button */
#define LED_GPIO   8
#define UI_TICK_MS 125

/* Tasks: the reader outranks the server so P1 bytes are never starved. */
#define P1_READER_TASK_STACK  4096
#define P1_READER_TASK_PRIO   6
#define TCP_SERVER_TASK_STACK 4096
#define TCP_SERVER_TASK_PRIO  5

/* Supervisor (normal mode) */
#define HEARTBEAT_PERIOD_S      60
#define OFFLINE_RESTART_S       (15 * 60)
#define NO_CLIENT_RESTART_S     (30 * 60)   /* online, readers seen since boot, but none now */
#define MIN_HEAP_BLOCK          (16 * 1024) /* largest free block below this: restart */

/* WiFi setup portal */
#define SETUP_BUTTON_GPIO       9 /* BOOT, active low */
#define SETUP_HOLD_S            5
#define SETUP_AP_SSID           "Piepr-Setup"
#define SETUP_AP_MAX_STATIONS   2
#define SETUP_SCAN_MAX_APS      32 /* access points used to pick the hotspot channel */
#define SETUP_SURVEY_MAX        48 /* network names tracked while surveying */
#define SETUP_TIMEOUT_S         600
#define RECOVERY_SETUP_WINDOW_S 300 /* setup window after automatic recovery */
