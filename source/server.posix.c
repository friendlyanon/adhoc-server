#include "server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Server creation */

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

void set_socket_span(ah_server* server, ah_socket_span span)
{
  server->socket_span = span;
}

/* Socket creation */

typedef struct ah_socket {
  int socket;
} ah_socket;

ah_socket* span_get_socket(ah_server* server, size_t index)
{
  return &server->socket_span.sockets[index];
}

typedef struct ah_socket_slot {
  bool ok;
  ah_socket socket;
} ah_socket_slot;

size_t socket_size()
{
  return sizeof(ah_socket);
}

static ah_socket_slot create_unbound_socket(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  int unbound_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (unbound_socket < 0) {
    perror("socket");
    slot.ok = false;
  } else {
    slot.socket = unbound_socket;
  }

  return slot;
}

static ah_socket_slot socket_set_nonblocking(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  int socket_descriptor = slot.socket.socket;
  int flags = fcntl(socket_descriptor, F_GETFL);
  if (flags < 0 || fcntl(socket_descriptor, F_SETFL, flags | O_NONBLOCK) < 0) {
    perror("fcntl");
    slot.ok = false;
  }

  return slot;
}

static ah_socket_slot socket_enable_address_reuse(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  int enable = true;
  int result = setsockopt(
      slot.socket.socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  if (result < 0) {
    perror("setsockopt");
    slot.ok = false;
  }

  return slot;
}

static ah_socket_slot bind_socket(ah_socket_slot slot, uint16_t port)
{
  if (!slot.ok) {
    return slot;
  }

  struct sockaddr_in address = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = INADDR_ANY},
  };
  const struct sockaddr* address_ptr = (const struct sockaddr*)&address;
  if (bind(slot.socket.socket, address_ptr, sizeof(address)) < 0) {
    perror("bind");
    slot.ok = false;
  }

  return slot;
}

static ah_socket_slot listen_on_socket(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  if (listen(slot.socket.socket, SOMAXCONN) < 0) {
    perror("listen");
    slot.ok = false;
  }

  return slot;
}

bool create_socket(ah_socket* result_socket, ah_server* server, uint16_t port)
{
  (void)server;

  ah_socket_slot slot = {true, {-1}};
  slot = create_unbound_socket(slot);
  slot = socket_set_nonblocking(slot);
  slot = socket_enable_address_reuse(slot);
  slot = bind_socket(slot, port);
  slot = listen_on_socket(slot);

  memcpy(result_socket, &slot.socket, socket_size());
  return slot.ok;
}

/* Socket destruction */

bool destroy_socket(ah_socket* socket)
{
  if (socket->socket == -1) {
    return true;
  }

  if (close(socket->socket) != 0) {
    perror("close");
    return false;
  }

  socket->socket = -1;
  return true;
}
