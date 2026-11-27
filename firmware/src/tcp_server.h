#pragma once

#include <stddef.h>
#include <stdint.h>

/* Starts the server task: listens on TCP_PORT, max TCP_MAX_CLIENTS clients. */
void tcp_server_start(void);

/* Sends data to every connected client (thread-safe, never blocks).
 * A client that cannot take the whole buffer at once is dropped. */
void tcp_server_broadcast(const uint8_t *data, size_t len);

int tcp_server_client_count(void);
