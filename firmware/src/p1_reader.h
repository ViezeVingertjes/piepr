#pragma once

#include "p1_status.h"

/* Installs the UART on P1_RX_GPIO (inverted, 115200 8N1) and starts the reader
 * task: valid telegrams go to tcp_server_broadcast() and flicker the LED, and
 * their values are kept for p1_reader_status(). */
void p1_reader_start(void);

/* The counts since start and the values of the last valid telegram, as they
 * were at one moment. Any task, once the reader is started; never waits longer
 * than a copy of the values takes. */
void p1_reader_status(p1_status_t *out);
