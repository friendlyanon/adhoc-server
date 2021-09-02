#include "server.nt.h"

#include <MSWSock.h>
#include <stdio.h>
#include <string.h>

#define ERROR_MESSAGE_SIZE 256

static void print_error(const char* function, int error_code)
{
  char error_message[ERROR_MESSAGE_SIZE];
  error_message[0] = '\0';

  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 error_code,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 error_message,
                 ERROR_MESSAGE_SIZE,
                 NULL);

  if (error_message[0] == '\0') {
    fprintf(stderr, "%s: 0x%08X\n", function, (unsigned int)error_code);
  } else {
    fprintf(stderr, "%s: %s\n", function, error_message);
  }
}

/* Server creation */

typedef struct ah_server {
  bool server_started;
  HANDLE completion_port;
  ah_socket_span socket_span;
} ah_server;

typedef struct ah_server_slot {
  bool ok;
  ah_server server;
} ah_server_slot;

size_t server_size()
{
  return sizeof(ah_server);
}

static ah_server_slot startup(ah_server_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  WSADATA wsa_data;
  int startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    print_error("WSAStartup", startup_result);
    slot.ok = false;
  } else {
    slot.server.server_started = true;

    int major = LOBYTE(wsa_data.wVersion);
    int minor = HIBYTE(wsa_data.wVersion);
    if (major != 2 || minor != 2) {
      fprintf(stderr,
              "Could not find a usable version of Winsock.dll\n"
              "  Expected: 2.2\n"
              "  Actual: %d.%d\n",
              major,
              minor);
      slot.ok = false;
    }
  }

  return slot;
}

static ah_server_slot create_completion_port(ah_server_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  HANDLE completion_port =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (completion_port == NULL) {
    print_error("CreateIoCompletionPort", GetLastError());
    slot.ok = false;
  } else {
    slot.server.completion_port = completion_port;
  }

  return slot;
}

bool create_server(ah_server* result_server)
{
  ah_server_slot slot = {true, {.completion_port = INVALID_HANDLE_VALUE}};
  slot = startup(slot);
  slot = create_completion_port(slot);

  memcpy(result_server, &slot.server, server_size());
  return slot.ok;
}

void set_socket_span(ah_server* server, ah_socket_span span)
{
  server->socket_span = span;
}

/* Socket creation */

typedef struct ah_socket {
  SOCKET socket;
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

  SOCKET unbound_socket = WSASocket(
      AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (unbound_socket == INVALID_SOCKET) {
    print_error("WSASocket", WSAGetLastError());
    slot.ok = false;
  } else {
    slot.socket.socket = unbound_socket;
  }

  return slot;
}

static ah_socket_slot register_socket(ah_socket_slot slot, ah_server* server)
{
  if (!slot.ok) {
    return slot;
  }

  _Static_assert(sizeof(HANDLE) == sizeof(SOCKET),
                 "HANDLE and SOCKET differ in size");
  HANDLE socket_handle = NULL;
  memcpy(&socket_handle, &slot.socket.socket, sizeof(SOCKET));

  HANDLE result =
      CreateIoCompletionPort(socket_handle, server->completion_port, 0, 0);
  if (result == NULL) {
    print_error("CreateIoCompletionPort", GetLastError());
    slot.ok = false;
  }

  return slot;
}

static ah_socket_slot socket_enable_address_reuse(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  BOOL enable = TRUE;
  int result = setsockopt(slot.socket.socket,
                          SOL_SOCKET,
                          SO_REUSEADDR,
                          (const char*)&enable,
                          sizeof(enable));
  if (result == SOCKET_ERROR) {
    print_error("setsockopt", WSAGetLastError());
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
  int result = bind(
      slot.socket.socket, (const struct sockaddr*)&address, sizeof(address));
  if (result == SOCKET_ERROR) {
    print_error("bind", WSAGetLastError());
    slot.ok = false;
  }

  return slot;
}

static ah_socket_slot listen_on_socket(ah_socket_slot slot)
{
  if (!slot.ok) {
    return slot;
  }

  if (listen(slot.socket.socket, SOMAXCONN) == SOCKET_ERROR) {
    print_error("listen", WSAGetLastError());
    slot.ok = false;
  }

  return slot;
}

bool create_socket(ah_socket* result_socket, ah_server* server, uint16_t port)
{
  ah_socket_slot slot = {true, {INVALID_SOCKET}};
  slot = create_unbound_socket(slot);
  slot = register_socket(slot, server);
  slot = socket_enable_address_reuse(slot);
  slot = bind_socket(slot, port);
  slot = listen_on_socket(slot);

  memcpy(result_socket, &slot.socket, socket_size());
  return slot.ok;
}
