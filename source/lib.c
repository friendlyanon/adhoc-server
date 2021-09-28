#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

#define KILOBYTES(n) (1024 * (n))

static uint8_t ez_buffer[KILOBYTES(2)] = {0};
static uint8_t* ez_buffer_pointer = ez_buffer;

static void* ez_malloc(size_t size)
{
  uint8_t* pointer = ez_buffer_pointer;
  ez_buffer_pointer += size;
  return pointer;
}

typedef struct io_session {
  ah_io_dock dock;
  ah_socket_accepted socket;
  ah_ipv4_address address;
  uint8_t buffer[KILOBYTES(8)];
} io_session;

static bool on_write_complete(ah_error_code error_code,
                              ah_io_operation* operation,
                              uint32_t bytes_transferred)
{
  (void)error_code;
  (void)bytes_transferred;

  io_session* session = (io_session*)dock_from_operation(operation);
  ah_socket_accepted* socket = &session->socket;

  bool* stop_server = context_from_socket(socket)->user_data;
  *stop_server = true;

  bool result = destroy_socket(socket);
  ah_ipv4_address address = session->address;

  printf("Connection closed (%hhu.%hhu.%hhu.%hhu:%hu)\n",
         address.address[0],
         address.address[1],
         address.address[2],
         address.address[3],
         address.port);

  free(session);
  return result;
}

static bool on_read_complete(ah_error_code error_code,
                             ah_io_operation* operation,
                             uint32_t bytes_transferred)
{
  (void)error_code;

  ah_io_buffer buffer = buffer_from_io_operation(operation);

  fwrite(buffer.buffer, 1, bytes_transferred, stdout);

  buffer.buffer_length = bytes_transferred;
  return queue_write_operation(
      dock_from_operation(operation), buffer, on_write_complete);
}

static bool on_accept(ah_error_code error_code,
                      ah_socket* socket,
                      ah_ipv4_address address)
{
  (void)error_code;

  printf("New connection from %hhu.%hhu.%hhu.%hhu:%hu\n",
         address.address[0],
         address.address[1],
         address.address[2],
         address.address[3],
         address.port);

  io_session* session = calloc(1, sizeof(io_session));
  if (session == NULL) {
    return false;
  }

  move_socket(&session->socket, socket);
  session->dock.socket = &session->socket;
  session->address = address;

  return queue_read_operation(
      &session->dock,
      (ah_io_buffer) {sizeof(session->buffer), session->buffer},
      on_read_complete);
}

library create_library()
{
  ah_server* server = ez_malloc(server_size());
  if (!create_server(server)) {
    goto exit;
  }

  bool stop_server = false;
  ah_socket* socket = ez_malloc(socket_size());
  set_socket_span(server, (ah_socket_span) {1, socket});
  ah_context context = {server, &stop_server};
  {
    const uint16_t port = 1337;
    if (!create_socket(socket, &context, port)) {
      goto exit;
    }
  }

  ah_acceptor* acceptor = ez_malloc(acceptor_size());
  if (!create_acceptor(acceptor, socket, on_accept)) {
    goto exit;
  }

  while (1) {
    if (!server_tick(server, NULL) || stop_server) {
      break;
    }
  }

exit:
  destroy_server(server);
  ez_buffer_pointer = ez_buffer;
  return (library) {"adhoc-server"};
}
