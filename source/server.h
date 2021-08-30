#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ah_server ah_server;
typedef struct ah_socket ah_socket;

/**
 * @brief Returns the size of the buffer to be allocated for ::create_server.
 */
size_t server_size(void);

/**
 * @brief Creates the server and writes it out to the \c result_server buffer.
 */
bool create_server(ah_server* result_server);

/**
 * @brief Returns the size of the buffer to be allocated for ::create_socket.
 */
size_t socket_size(void);

/**
 * @brief Create a TCP/IPv4 socket bound to and listening on \c port.
 */
bool create_socket(ah_socket* result_socket, uint16_t port);
