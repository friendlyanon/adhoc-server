#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "error_code.h"

typedef struct ah_server ah_server;
typedef struct ah_socket ah_socket;
typedef struct ah_acceptor ah_acceptor;

typedef struct ah_socket_span {
  size_t size;
  ah_socket* sockets;
} ah_socket_span;

typedef struct ah_ipv4_address {
  uint8_t address[4];
  uint16_t port;
} ah_ipv4_address;

typedef struct ah_socket_accepted {
  uint8_t reserved[AH_SOCKET_ACCEPTED_SIZE];
} ah_socket_accepted;

typedef bool (*ah_on_accept)(ah_error_code error_code,
                             ah_server* server,
                             ah_socket* socket,
                             ah_ipv4_address address,
                             void* user_data);

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
 * @brief Closes the provided socket.
 */
bool destroy_socket(ah_server* server, ah_socket* socket);

/**
 * @brief Closes the provided socket that was taken ownership of in an accept
 * handler.
 */
bool destroy_socket_accepted(ah_server* server, ah_socket_accepted* socket);

/**
 * @brief Sets the internal socket span to the one provided.
 */
void set_socket_span(ah_server* server, ah_socket_span span);

/**
 * @brief Returns a pointer to the socket at \c index.
 */
ah_socket* span_get_socket(ah_server* server, size_t index);

/**
 * @brief Returns the size of the buffer to be allocated for ::create_acceptor.
 */
size_t acceptor_size(void);

/**
 * @brief Creates an acceptor that queues an accept operation in the server.
 */
bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_server* server,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept,
                     void* user_data);

/**
 * @brief Drives the <tt>server</tt>'s event loop and calls the event handlers.
 *
 * This function calls the platform dependent event loop function to dequeue
 * events and dispatches on those events. If this function fails, then the OS
 * native error code will be returned via the \c error_code parameter. If \c
 * error_code is \c NULL, then the error will be printed to stderr.
 */
bool server_tick(ah_server* server, int* error_code);

/**
 * @brief Takes the ownership of an accepted socket from the server in an
 * ::ah_on_accept callback.
 */
void move_socket(ah_socket_accepted* result_socket, ah_socket* socket);
