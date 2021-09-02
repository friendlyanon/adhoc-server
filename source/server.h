#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ah_server ah_server;
typedef struct ah_socket ah_socket;

typedef struct ah_socket_span {
  size_t size;
  ah_socket* sockets;
} ah_socket_span;

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
 * @brief Creates a TCP/IPv4 socket bound to and listening on \c port.
 */
bool create_socket(ah_socket* result_socket, ah_server* server, uint16_t port);

/**
 * @brief Sets the internal socket span to the one provided.
 */
void set_socket_span(ah_server* server, ah_socket_span span);

/**
 * @brief Returns a pointer to the socket at \c index.
 */
ah_socket* span_get_socket(ah_server* server, size_t index);
