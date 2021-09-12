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
  ah_ipv4_address address;
  uint32_t length;
  uint8_t buffer[KILOBYTES(8)];
} io_operation;

#define IO_OP_FROM_SOCKET(socket_) \
  (io_operation*)((char*)(socket_)-offsetof(io_operation, socket))

static bool on_write_complete(ah_error_code error_code,
                              ah_socket_accepted* socket,
                              uint32_t bytes_transferred)
{
  (void)error_code;
  (void)bytes_transferred;

  io_operation* op = IO_OP_FROM_SOCKET(socket);
  bool result = destroy_socket(&op->socket);
  ah_ipv4_address address = op->address;

  printf("Connection closed (%hhu.%hhu.%hhu.%hhu:%hu)\n",
         address.address[0],
         address.address[1],
         address.address[2],
         address.address[3],
         address.port);

  free(op);
  return result;
}

static bool on_read_complete(ah_error_code error_code,
                             ah_socket_accepted* socket,
                             uint32_t bytes_transferred)
{
  (void)error_code;

  io_operation* op = IO_OP_FROM_SOCKET(socket);

  fwrite(op->buffer, 1, bytes_transferred, stdout);

  return queue_write_operation(
      socket, op->buffer, bytes_transferred, on_write_complete);
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

  io_operation* op = malloc(sizeof(io_operation));
  if (op == NULL) {
    return false;
  }

  move_socket(&op->socket, socket);
  *op = (io_operation) {op->socket, address, .length = 0};

  return queue_read_operation(
      &op->socket, op->buffer, KILOBYTES(8), on_read_complete);
}

library create_library()
{
  ah_server* server = ez_malloc(server_size());
  if (!create_server(server)) {
    goto cleanup_server;
  }

  ah_socket* socket = ez_malloc(socket_size());
  set_socket_span(server, (ah_socket_span) {1, socket});
  ah_context context = {server, NULL};
  {
    const uint16_t port = 1337;
    if (!create_socket(socket, &context, port)) {
      goto cleanup_server;
    }
  }

  ah_acceptor* acceptor = ez_malloc(acceptor_size());
  if (!create_acceptor(acceptor, socket, on_accept)) {
    goto exit;
  }

  while (1) {
    if (!server_tick(server, NULL)) {
      goto exit;
    }
  }

exit:
  destroy_acceptor(acceptor);
cleanup_server:
  destroy_server(server);
  ez_buffer_pointer = ez_buffer;
  return (library) {"adhoc-server"};
}
