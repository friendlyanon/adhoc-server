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

static bool on_accept(ah_ipv4_address address, ah_socket* socket)
{
  (void)socket;

  printf("New connection from %d.%d.%d.%d\n",
         (int)address.address[0],
         (int)address.address[1],
         (int)address.address[2],
         (int)address.address[3]);

  return true;
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
  if (!create_acceptor(acceptor, server, socket, on_accept)) {
    goto exit;
  }

  while (1) {
    if (!server_tick(server)) {
      goto exit;
    }
  }

exit:
  return (library) {"adhoc-server"};
}
