#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

size_t server_size()
{
  return 0;
}

bool create_server(void* result_server)
{
  (void)result_server;

  return true;
}

typedef struct ah_socket {
  bool ok;
  int socket;
} ah_socket;

size_t socket_size()
{
  return sizeof(ah_socket);
}

static ah_socket create_unbound_socket(ah_socket socket)
{
  if (!socket.ok) {
    return socket;
  }

  int unbound_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (unbound_socket < 0) {
    perror("socket");
    socket.ok = false;
  } else {
    socket.socket = unbound_socket;
  }

  return socket;
}

static ah_socket bind_socket(ah_socket socket, int port)
{
  if (!socket.ok) {
    return socket;
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = INADDR_ANY},
  };
  const struct sockaddr* address_ptr = (const struct sockaddr*)&address;
  if (bind(socket.socket, address_ptr, sizeof(address)) < 0) {
    perror("bind");
    socket.ok = false;
  }

  return socket;
}

/**
 * POSIX does not provide a macro similar to SOMAXCONN from Windows.
 */
#define SOMAXCONN 5

static ah_socket listen_on_socket(ah_socket socket)
{
  if (!socket.ok) {
    return socket;
  }

  if (listen(socket.socket, SOMAXCONN) < 0) {
    perror("listen");
    socket.ok = false;
  }

  return socket;
}

bool create_socket(void* result_socket, int port)
{
  ah_socket socket = {.ok = true, .socket = -1};
  socket = create_unbound_socket(socket);
  socket = bind_socket(socket, port);
  socket = listen_on_socket(socket);

  memcpy(result_socket, &socket, socket_size());
  return socket.ok;
}
