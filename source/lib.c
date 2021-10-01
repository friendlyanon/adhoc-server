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
  size_t state;
  ah_socket_accepted socket;
  ah_ipv4_address address;
  uint32_t bytes_read;
  uint8_t buffer[KILOBYTES(8)];
} io_session;

static bool finish_session(io_session* session)
{
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

static bool coroutine(ah_error_code error_code,
                      ah_io_operation* operation,
                      uint32_t bytes_transferred,
                      void* per_call_data)
{
  (void)error_code;
  (void)per_call_data;

  ah_io_dock* dock = dock_from_operation(operation);
  io_session* session = (io_session*)dock;
  switch (session->state++) {
    case 0:
      /* fallthrough */
    case 1: {
      if (operation == &dock->read_port) {
        session->bytes_read = bytes_transferred;
        fwrite(session->buffer, 1, bytes_transferred, stdout);
      }
      if (session->state == 2) {
        return queue_write_operation(
            dock,
            (ah_io_buffer) {session->bytes_read, session->buffer},
            coroutine,
            NULL);
      }
      break;
    }
    case 2:
      return finish_session(session);
  }

  return true;
}

#define BUFFER_FROM_STR(str) ((ah_io_buffer) {sizeof(str) - 1, (str)})
#define BUFFER_FROM_ARR(arr) ((ah_io_buffer) {sizeof(arr), (arr)})

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

  return queue_write_operation(
             &session->dock, BUFFER_FROM_STR("Accepted\r\n"), coroutine, NULL)
      && queue_read_operation(
             &session->dock, BUFFER_FROM_ARR(session->buffer), coroutine, NULL);
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
