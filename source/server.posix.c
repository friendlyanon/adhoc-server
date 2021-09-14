#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* Server creation */

#define MAX_EVENTS 128

typedef struct ah_server {
  ah_socket_span socket_span;
  int epoll_descriptor;
  struct epoll_event events[MAX_EVENTS];
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
  int flags = fcntl(descriptor, F_GETFD);
  if (flags < 0 || fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) < 0) {
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

typedef enum ah_socket_role
{
  AH_SOCKET_ACCEPT = 0,
  AH_SOCKET_IO,
  AH_SOCKET_IO_REARM,
} ah_socket_role;

typedef struct ah_socket {
  int socket;
  ah_socket_role role;
  ah_context* context;
  void* on_complete;
  void* pointer;
  uint32_t length;
} ah_socket;

_Static_assert(
    sizeof(ah_socket) == sizeof(ah_socket_accepted),
    "AH_SOCKET_ACCEPTED_SIZE does not match the size of 'ah_socket'");

ah_socket* span_get_socket(ah_server* server, size_t index)
{
  ah_socket_span span = server->socket_span;
  return span.size <= index ? NULL : &span.sockets[index];
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

static ah_socket_slot socket_set_nonblocking(ah_socket_slot slot, bool report)
{
  if (!slot.ok) {
    return slot;
  }

  int socket_descriptor = slot.socket.socket;
  int flags = fcntl(socket_descriptor, F_GETFL);
  if (flags < 0 || fcntl(socket_descriptor, F_SETFL, flags | O_NONBLOCK) < 0) {
    if (report) {
      perror("fcntl");
    }

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
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
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

static ah_socket_slot queue_socket(ah_socket_slot slot,
                                   ah_context* context,
                                   ah_socket* result_socket)
{
  if (!slot.ok) {
    return slot;
  }

  int socket = slot.socket.socket;
  int epoll_descriptor = context->server->epoll_descriptor;
#ifdef EPOLLEXCLUSIVE
  uint32_t events = EPOLLIN | EPOLLEXCLUSIVE;
#else
  uint32_t events = EPOLLIN | EPOLLET | EPOLLONESHOT;
#endif
  struct epoll_event event = {events, .data.ptr = result_socket};
  if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, socket, &event) == -1) {
    perror("epoll_ctl");
    slot.ok = false;
  }

  return slot;
}

bool create_socket(ah_socket* result_socket, ah_context* context, uint16_t port)
{
  ah_socket_slot slot = {true, {.socket = -1, AH_SOCKET_ACCEPT, context}};
  slot = create_unbound_socket(slot);
  slot = socket_set_nonblocking(slot, true);
  slot = socket_enable_address_reuse(slot);
  slot = bind_socket(slot, port);
  slot = listen_on_socket(slot);
  slot = queue_socket(slot, context, result_socket);

  memcpy(result_socket, &slot.socket, socket_size());
  return slot.ok;
}

/* Acceptor destruction */

bool destroy_server(ah_server* server)
{
  bool result = true;

  ah_socket_span span = server->socket_span;
  for (size_t i = 0, size = span.size; i != size; ++i) {
    result = destroy_socket(&span.sockets[i]) && result;
  }

  if (server->epoll_descriptor != -1 && close(server->epoll_descriptor) == -1) {
    perror("close");
    result = false;
  }

  server->epoll_descriptor = -1;
  return true;
}

/* Context retrieval */

ah_context* context_from_socket_base(ah_socket* socket)
{
  return socket->context;
}

ah_context* context_from_socket_accepted(ah_socket_accepted* socket)
{
  return context_from_socket_base((ah_socket*)socket);
}

/* Socket destruction */

bool destroy_socket_base(ah_socket* socket)
{
  if (socket->socket == -1) {
    return true;
  }

  ah_server* server = context_from_socket(socket)->server;
  int result =
      epoll_ctl(server->epoll_descriptor, EPOLL_CTL_DEL, socket->socket, NULL);
  if (result == -1 && errno != ENOENT) {
    perror("epoll_ctl");
    return false;
  }

  if (close(socket->socket) != 0) {
    perror("close");
    return false;
  }

  socket->socket = -1;
  return true;
}

bool destroy_socket_accepted(ah_socket_accepted* socket)
{
  return destroy_socket_base((ah_socket*)socket);
}

/* Acceptor creation */

typedef struct ah_acceptor {
  char dummy;
} ah_acceptor;

size_t acceptor_size()
{
  return sizeof(ah_acceptor);
}

static bool accept_error_handler(ah_context* context,
                                 ah_on_accept on_accept,
                                 const char* function)
{
  int error_code = errno;
  if (!is_ah_error_code(error_code)) {
    perror(function);
    return false;
  }

  ah_socket_slot slot = {false, {.context = context}};
  return on_accept(
      (ah_error_code)error_code, &slot.socket, (ah_ipv4_address) {0});
}

static bool accept_handler(ah_socket* socket)
{
  ah_on_accept on_accept = (ah_on_accept)socket->on_complete;
  ah_context* context = context_from_socket(socket);
  struct sockaddr_in remote_address = {0};
  socklen_t remote_address_length = sizeof(remote_address);
  int incoming_socket = accept(socket->socket,
                               (struct sockaddr*)&remote_address,
                               &remote_address_length);
  if (incoming_socket == -1) {
    return accept_error_handler(context, on_accept, "accept");
  }

#ifndef EPOLLEXCLUSIVE
  {
    uint32_t events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    struct epoll_event event = {events, .data.ptr = socket};
    int result = epoll_ctl(context->server->epoll_descriptor,
                           EPOLL_CTL_MOD,
                           socket->socket,
                           &event);
    if (result == -1) {
      return accept_error_handler(context, on_accept, "epoll_ctl");
    }
  }
#endif

  ah_socket_slot slot = {
      .ok = true,
      {.socket = incoming_socket, AH_SOCKET_IO, context},
  };
  slot = socket_set_nonblocking(slot, false);
  if (!slot.ok) {
    return accept_error_handler(context, on_accept, "fcntl");
  }

  uint32_t address_raw = ntohl(remote_address.sin_addr.s_addr);
  ah_ipv4_address address = {
      {address_raw >> 24 & 0xFF,
       address_raw >> 16 & 0xFF,
       address_raw >> 8 & 0xFF,
       address_raw & 0xFF},
      ntohs(remote_address.sin_port),
  };
  bool result = on_accept(AH_ERR_OK, &slot.socket, address);
  /* If ownership of the socket wasn't taken by the handler, then it gets
   * destroyed */
  if (slot.ok) {
    result = destroy_socket(&slot.socket) && result;
  }

  return result;
}

bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept)
{
  listening_socket->on_complete = (void*)on_accept;
  *result_acceptor = (ah_acceptor) {0};
  return true;
}

/* I/O */

void move_socket(ah_socket_accepted* result_socket, ah_socket* socket)
{
  ah_socket_slot* slot =
      (ah_socket_slot*)((char*)socket - offsetof(ah_socket_slot, socket));
  if (slot->ok) {
    memcpy(result_socket, socket, socket_size());
    slot->ok = false;
  }
}

static bool register_io_socket(ah_socket* socket, uint32_t events, bool rearm)
{
  events |= EPOLLERR | EPOLLHUP | EPOLLET | EPOLLONESHOT;

  int epoll_descriptor = context_from_socket(socket)->server->epoll_descriptor;
  struct epoll_event event = {events, .data.ptr = socket};
  int operation = rearm ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
  if (epoll_ctl(epoll_descriptor, operation, socket->socket, &event) == -1) {
    perror("epoll_ctl");
    return false;
  }

  return true;
}

static bool read_handler(ah_socket* socket)
{
  ah_on_io_complete on_complete = (ah_on_io_complete)socket->on_complete;
  ssize_t bytes_transferred =
      recv(socket->socket, socket->pointer, socket->length, 0);

  int error_code = 0;
  if (bytes_transferred == -1) {
    error_code = errno;
    if (!is_ah_error_code(error_code)) {
      perror("recv");
      return false;
    }

    bytes_transferred = 0;
  }

  return on_complete((ah_error_code)error_code,
                     (ah_socket_accepted*)socket,
                     (uint32_t)bytes_transferred);
}

bool queue_read_operation(ah_socket_accepted* accepted_socket,
                          void* output_buffer,
                          uint32_t buffer_length,
                          ah_on_io_complete on_complete)
{
  if (buffer_length > (uint32_t)INT32_MAX) {
    return false;
  }

  ah_socket* socket = (ah_socket*)accepted_socket;
  socket->on_complete = (void*)on_complete;
  socket->pointer = output_buffer;
  socket->length = buffer_length;

  bool result =
      register_io_socket(socket, EPOLLIN, socket->role == AH_SOCKET_IO_REARM);
  socket->role = AH_SOCKET_IO_REARM;
  return result;
}

static bool write_handler(ah_socket* socket)
{
  ah_on_io_complete on_complete = (ah_on_io_complete)socket->on_complete;
  ssize_t bytes_transferred =
      send(socket->socket, socket->pointer, socket->length, 0);

  int error_code = 0;
  if (bytes_transferred == -1) {
    error_code = errno;
    if (!is_ah_error_code(error_code)) {
      perror("send");
      return false;
    }

    bytes_transferred = 0;
  }

  return on_complete((ah_error_code)error_code,
                     (ah_socket_accepted*)socket,
                     (uint32_t)bytes_transferred);
}

bool queue_write_operation(ah_socket_accepted* accepted_socket,
                           void* input_buffer,
                           uint32_t buffer_length,
                           ah_on_io_complete on_complete)
{
  if (buffer_length > (uint32_t)INT32_MAX) {
    return false;
  }

  ah_socket* socket = (ah_socket*)accepted_socket;
  socket->on_complete = (void*)on_complete;
  socket->pointer = input_buffer;
  socket->length = buffer_length;

  bool result =
      register_io_socket(socket, EPOLLOUT, socket->role == AH_SOCKET_IO_REARM);
  socket->role = AH_SOCKET_IO_REARM;
  return result;
}

/* Event loop */

bool server_tick(ah_server* server, int* error_code_out)
{
  int new_events =
      epoll_wait(server->epoll_descriptor, server->events, MAX_EVENTS, -1);
  if (new_events == -1) {
    if (error_code_out == NULL) {
      perror("epoll_wait");
    } else {
      *error_code_out = errno;
    }

    return false;
  }

  for (int i = 0; i < new_events; ++i) {
    struct epoll_event* event = &server->events[i];
    ah_socket* socket = (ah_socket*)event->data.ptr;
    if (socket->role == AH_SOCKET_ACCEPT) {
      if (!accept_handler(socket)) {
        return false;
      }
    } else {
      bool result = (event->events & EPOLLIN) != 0 ? read_handler(socket)
                                                   : write_handler(socket);
      if (!result) {
        return false;
      }
    }
  }

  return true;
}
