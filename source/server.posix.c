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

#define MAX_EVENTS 128

typedef struct ah_server {
  ah_socket_span socket_span;
  struct epoll_event events[MAX_EVENTS];
  int epoll_descriptor;
} ah_server;

size_t server_size()
{
  return sizeof(ah_server);
}

bool create_server(ah_server* result_server)
{
#ifdef EPOLL_CLOEXEC
  int descriptor = epoll_create1(EPOLL_CLOEXEC);
#else
  int descriptor = epoll_create(1);
#endif

  *result_server = (ah_server) {.epoll_descriptor = descriptor};
  if (descriptor < 0) {
#ifdef EPOLL_CLOEXEC
    perror("epoll_create1");
#else
    perror("epoll_create");
#endif

    return false;
  }

#ifndef EPOLL_CLOEXEC
  if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
    perror("fcntl");
    return false;
  }
#endif

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
  ah_socket_span span = server->socket_span;
  return span.size < index ? NULL : &span.sockets[index];
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
    slot.socket.socket = unbound_socket;
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

/* Acceptor creation */

typedef struct ah_acceptor {
  char dummy;
} ah_acceptor;

size_t acceptor_size()
{
  return sizeof(ah_acceptor);
}

bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_server* server,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept)
{
  (void)server;
  (void)listening_socket;
  (void)on_accept;

  *result_acceptor = (ah_acceptor) {0};
  return true;
}

/* Event loop */

bool server_tick(ah_server* server)
{
  (void)server;

  return true;
}
