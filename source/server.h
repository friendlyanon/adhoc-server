#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "server/error_code.h"

typedef struct ah_server ah_server;
typedef struct ah_socket ah_socket;
typedef struct ah_acceptor ah_acceptor;

typedef struct ah_context {
  ah_server* server;
  void* user_data;
} ah_context;

typedef struct ah_socket_span {
  size_t size;
  ah_socket* sockets;
} ah_socket_span;

typedef struct ah_ipv4_address {
  uint8_t address[4];
  uint16_t port;
} ah_ipv4_address;

typedef struct ah_socket_accepted {
  _Alignas(
      AH_SOCKET_ACCEPTED_ALIGNMENT) uint8_t reserved[AH_SOCKET_ACCEPTED_SIZE];
} ah_socket_accepted;

typedef struct ah_io_operation {
  _Alignas(AH_IO_OPERATION_ALIGNMENT) uint8_t reserved[AH_IO_OPERATION_SIZE];
} ah_io_operation;

typedef struct ah_io_dock {
  ah_socket_accepted* socket;
  ah_io_operation read_port;
  ah_io_operation write_port;
} ah_io_dock;

typedef struct ah_io_buffer {
  uint32_t buffer_length;
  void* buffer;
} ah_io_buffer;

/**
 * @brief Callback type for async accept operation.
 *
 * The \c socket parameter must be taken ownership of using the ::move_socket
 * function to perform async I/O operations on it. Failing to take ownership of
 * the socket will result in its closure after the callback returns.
 */
typedef bool (*ah_on_accept)(ah_error_code error_code,
                             ah_socket* socket,
                             ah_ipv4_address address);

/**
 * @brief Callback type for the async I/O operations.
 *
 * Although the \c bytes_transferred parameter is an unsigned integer, this
 * value will not be greater than \c INT32_MAX (2147483647). However, it can be
 * 0 sometimes, when there is a pending empty packet to be read.
 */
typedef bool (*ah_on_io_complete)(ah_error_code error_code,
                                  ah_io_operation* operation,
                                  uint32_t bytes_transferred,
                                  void* per_call_data);

/**
 * @brief Returns the size of the ::ah_server object.
 */
size_t server_size(void);

/**
 * @brief Returns the alignment of the ::ah_server object.
 */
size_t server_alignment(void);

/**
 * @brief Creates the server and writes it out to the \c result_server buffer.
 */
bool create_server(ah_server* result_server);

/**
 * @brief Destroys the provided server.
 */
bool destroy_server(ah_server* server);

/**
 * @brief Returns the size of the ::ah_socket object.
 */
size_t socket_size(void);

/**
 * @brief Returns the alignment of the ::ah_socket object.
 */
size_t socket_alignment(void);

/**
 * @brief Creates a TCP/IPv4 socket bound to and listening on \c port.
 */
bool create_socket(ah_socket* result_socket,
                   ah_context* context,
                   uint16_t port);

/**
 * @brief Closes the provided socket.
 */
bool destroy_socket_base(ah_socket* socket);

/**
 * @brief Closes the provided socket that was taken ownership of in an accept
 * handler.
 */
bool destroy_socket_accepted(ah_socket_accepted* socket);

/* clang-format off */

/**
 * @brief Type generic macro function that destroys the socket.
 */
#define destroy_socket(socket) \
  _Generic((socket), \
           ah_socket*: destroy_socket_base, \
           ah_socket_accepted*: destroy_socket_accepted)(socket)

/* clang-format on */

/**
 * @brief Sets the internal socket span to the one provided.
 */
void set_socket_span(ah_server* server, ah_socket_span span);

/**
 * @brief Returns a pointer to the socket at \c index.
 */
ah_socket* span_get_socket(ah_server* server, size_t index);

/**
 * @brief Returns the size of the ::ah_acceptor object.
 */
size_t acceptor_size(void);

/**
 * @brief Returns the alignment of the ::ah_acceptor object.
 */
size_t acceptor_alignment(void);

/**
 * @brief Creates an acceptor that queues an accept operation in the server.
 */
bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept);

/**
 * @brief Drives the <tt>server</tt>'s event loop and calls the event handlers.
 *
 * This function calls the platform dependent event loop function to dequeue
 * events and dispatches on those events. If this function fails, then the OS
 * native error code will be returned via the \c error_code_out parameter. If
 * \c error_code_out is \c NULL, then the error will be printed to stderr.
 */
bool server_tick(ah_server* server, int* error_code_out);

/**
 * @brief Takes the ownership of an accepted socket from the server in an
 * ::ah_on_accept callback.
 */
void move_socket(ah_socket_accepted* result_socket, ah_socket* socket);

/**
 * @brief Returns the ::ah_context pointer from the socket.
 */
ah_context* context_from_socket_base(ah_socket* socket);

/**
 * @brief Returns the ::ah_context pointer from the accepted socket.
 */
ah_context* context_from_socket_accepted(ah_socket_accepted* socket);

/* clang-format off */

/**
 * @brief Type generic macro function that returns the context from the socket.
 */
#define context_from_socket(socket) \
  _Generic((socket), \
           ah_socket*: context_from_socket_base, \
           ah_socket_accepted*: context_from_socket_accepted)(socket)

/* clang-format on */

/**
 * @brief Returns whether the I/O operation is parked in a dock, i.e. active.
 *
 * User code must call this function before queueing any I/O operation. A
 * recommended way to deal with multiple operations of the same kind is to have
 * a FIFO queue in user code for storing the excess.
 */
bool is_io_operation_active(ah_io_operation* operation);

/**
 * @brief Returns the queued buffer.
 */
ah_io_buffer buffer_from_io_operation(ah_io_operation* operation);

/**
 * @brief Returns the dock corresponding to the I/O operation.
 */
ah_io_dock* dock_from_operation(ah_io_operation* operation);

/**
 * @brief Queues a read operation into the provided buffer.
 *
 * The buffer must stay alive for the duration of the operation. The provided
 * buffer's length MUST NOT be greater than \c INT32_MAX (2147483647). The
 * callback will also not receive a value greater than that for the number of
 * bytes transferred.
 */
bool queue_read_operation4(ah_io_dock* dock,
                           ah_io_buffer buffer,
                           ah_on_io_complete on_complete,
                           void* per_call_data);

/**
 * @brief Queues a write operation from the provided buffer.
 *
 * The buffer must stay alive for the duration of the operation. The provided
 * buffer's length MUST NOT be greater than \c INT32_MAX (2147483647). The
 * callback will also not receive a value greater than that for the number of
 * bytes transferred.
 */
bool queue_write_operation4(ah_io_dock* dock,
                            ah_io_buffer buffer,
                            ah_on_io_complete on_complete,
                            void* per_call_data);

/**
 * @brief Dispatches to ::queue_read_operation4 with the 4th argument as
 * \c NULL.
 */
#define queue_read_operation3(x, y, z) queue_read_operation4(x, y, z, NULL)

/**
 * @brief Dispatches to ::queue_write_operation4 with the 4th argument as
 * \c NULL.
 */
#define queue_write_operation3(x, y, z) queue_write_operation4(x, y, z, NULL)

#define AH__CONCAT_IMPL(x, y) x##y

#define AH__CONCAT(x, y) AH__CONCAT_IMPL(x, y)

#define AH__COUNT_AUX(_1, _2, _3, _4, x, ...) x

#define AH__COUNT(...) AH__COUNT_AUX(__VA_ARGS__, 4, 3, 2, 1, ~)

/**
 * @brief Dispatches to #queue_read_operation3 or ::queue_read_operation4 based
 * on the number of arguments.
 *
 * @note The arguments may be evaluated multiple times.
 */
#define queue_read_operation(...) \
  AH__CONCAT(queue_read_operation, AH__COUNT(__VA_ARGS__))(__VA_ARGS__)

/**
 * @brief Dispatches to #queue_write_operation3 or ::queue_write_operation4
 * based on the number of arguments.
 *
 * @note The arguments may be evaluated multiple times.
 */
#define queue_write_operation(...) \
  AH__CONCAT(queue_write_operation, AH__COUNT(__VA_ARGS__))(__VA_ARGS__)
