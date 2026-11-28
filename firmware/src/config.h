#pragma once

/* Application settings; pins are in board.h, module-internal constants in their .c file. */

#include "board.h"

#define US_PER_S 1000000LL

/* P1 port (DSMR 4/5: 115200 8N1, inverted), on BOARD_P1_RX_GPIO */
#define P1_BAUD 115200

/* Network */
#define HOSTNAME        "piepr"
#define TCP_PORT        8088
#define TCP_MAX_CLIENTS 5 /* a 6th replaces the longest-connected (see tcp_server.c) */

/* The tick that drives the status LED and polls the setup button */
#define UI_TICK_MS 125

/* Tasks: the reader outranks the server so P1 bytes are never starved. */
#define P1_READER_TASK_STACK  4096
#define P1_READER_TASK_PRIO   6
#define TCP_SERVER_TASK_STACK 4096
#define TCP_SERVER_TASK_PRIO  5

/* Supervisor (normal mode) */
#define HEARTBEAT_PERIOD_S  (5 * 60)
#define OFFLINE_RESTART_S   (15 * 60)
#define NO_CLIENT_RESTART_S (30 * 60)   /* online, readers seen since boot, but none now */
#define MIN_HEAP_BLOCK      (16 * 1024) /* largest free block below this: restart */

/* WiFi setup portal */
#define SETUP_HOLD_S            5 /* holding BOARD_SETUP_BUTTON_GPIO, where the board has one */
#define SETUP_AP_SSID           "Piepr-Setup"
#define SETUP_AP_MAX_STATIONS   2
#define SETUP_SCAN_MAX_APS      32 /* access points used to pick the hotspot channel */
#define SETUP_SURVEY_MAX        48 /* network names tracked while surveying */
#define SETUP_TIMEOUT_S         600
#define RECOVERY_SETUP_WINDOW_S 300 /* setup window after automatic recovery */

/* Log tags, one per module. main.c shows these at INFO and keeps ESP-IDF's own
 * components to warnings and errors. */
#define LOG_TAG_MAIN   "main"
#define LOG_TAG_CREDS  "wifi_creds"
#define LOG_TAG_NET    "net" /* not "wifi": that is the WiFi driver's own tag */
#define LOG_TAG_SETUP  "setup"
#define LOG_TAG_BUTTON "button"
#define LOG_TAG_MDNS   "mdns"
#define LOG_TAG_TCP    "tcp"
#define LOG_TAG_P1     "p1"
#define LOG_TAGS                                                                                   \
    { LOG_TAG_MAIN,   LOG_TAG_CREDS, LOG_TAG_NET, LOG_TAG_SETUP,                                   \
      LOG_TAG_BUTTON, LOG_TAG_MDNS,  LOG_TAG_TCP, LOG_TAG_P1 }
