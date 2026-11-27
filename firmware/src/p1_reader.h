#pragma once

/* Installs the UART on P1_RX_GPIO (inverted, 115200 8N1) and starts the reader
 * task: valid telegrams go to tcp_server_broadcast() and flicker the LED. */
void p1_reader_start(void);
