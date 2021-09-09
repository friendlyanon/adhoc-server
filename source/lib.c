#include <lib.h>
#include <server.h>
#include <stdio.h>
#include <stdlib.h>

#define KILOBYTES(n) (1024 * (n))

static uint8_t ez_buffer[KILOBYTES(2)] = {0};
static uint8_t* ez_buffer_pointer = ez_buffer;

static void* ez_malloc(size_t size)
{
  uint8_t* pointer = ez_buffer_pointer;
  ez_buffer_pointer += size;
  return pointer;
}

typedef struct io_operation {
  ah_socket_accepted socket;
} io_operation;

static bool on_accept(ah_error_code error_code,
                      ah_server* server,
                      ah_socket* socket,
                      ah_ipv4_address address,
                      void* user_data)
{
  (void)error_code;
  (void)user_data;

  printf("New connection from %hhu.%hhu.%hhu.%hhu:%hu\n",
         address.address[0],
         address.address[1],
         address.address[2],
         address.address[3],
         address.port);

  io_operation* op = malloc(sizeof(io_operation));
  if (op == NULL) {
    return false;
  }

  move_socket(&op->socket, socket);

  bool result = destroy_socket_accepted(server, &op->socket);
  free(op);

  return result;
}

library create_library()
{
  ah_server* server = ez_malloc(server_size());
  if (!create_server(server)) {
    goto exit;
  }

  ah_socket* socket = ez_malloc(socket_size());
  {
    const uint16_t port = 1337;
    if (!create_socket(socket, server, port)) {
      goto exit;
    }
  }
  set_socket_span(server, (ah_socket_span) {1, socket});

  ah_acceptor* acceptor = ez_malloc(acceptor_size());
  if (!create_acceptor(acceptor, server, socket, on_accept, NULL)) {
    goto exit;
  }

  while (1) {
    if (!server_tick(server, NULL)) {
      goto exit;
    }
  }

exit:
  return (library) {"adhoc-server"};
}
