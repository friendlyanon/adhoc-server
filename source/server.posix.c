#include "server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

typedef struct ah_server {
  ah_socket_span socket_span;
} ah_server;

size_t server_size()
{
  return sizeof(ah_server);
}

bool create_server(ah_server* result_server)
{
  memset(result_server, 0, server_size());
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

static ah_socket create_unbound_socket(ah_socket socket_)
{
  if (!socket_.ok) {
    return socket_;
  }

  int unbound_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (unbound_socket < 0) {
    perror("socket");
    socket_.ok = false;
  } else {
    socket_.socket = unbound_socket;
  }

  return socket_;
}

static ah_socket socket_set_nonblocking(ah_socket socket)
{
  if (!socket.ok) {
    return socket;
  }

  int flags = fcntl(socket.socket, F_GETFL);
  if (flags < 0 || fcntl(socket.socket, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("fcntl");
    socket.ok = false;
  }

  return socket;
}

static ah_socket socket_enable_address_reuse(ah_socket socket)
{
  if (!socket.ok) {
    return socket;
  }

  int enable = true;
  int result = setsockopt(
      socket.socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  if (result < 0) {
    perror("setsockopt");
    socket.ok = false;
  }

  return socket;
}

static ah_socket bind_socket(ah_socket socket, uint16_t port)
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

bool create_socket(ah_socket* result_socket, ah_server* server, uint16_t port)
{
  (void)server;

  ah_socket socket = {.ok = true, .socket = -1};
  socket = create_unbound_socket(socket);
  socket = socket_set_nonblocking(socket);
  socket = socket_enable_address_reuse(socket);
  socket = bind_socket(socket, port);
  socket = listen_on_socket(socket);

  memcpy(result_socket, &socket, socket_size());
  return socket.ok;
}

void set_socket_span(ah_server* server, ah_socket_span span)
{
  server->socket_span = span;
}

ah_socket* span_get_socket(ah_server* server, size_t index)
{
  return &server->socket_span.sockets[index];
}
