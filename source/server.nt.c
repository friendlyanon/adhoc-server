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

typedef struct ah_server {
  bool ok;
  bool server_started;
  HANDLE completion_port;
} ah_server;

size_t server_size()
{
  return sizeof(ah_server);
}

static ah_server startup(ah_server server)
{
  if (!server.ok) {
    return server;
  }

  WSADATA wsa_data;
  int startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    print_error("WSAStartup", startup_result);
    server.ok = false;
    return server;
  }
  server.server_started = true;

  if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
    fprintf(stderr,
            "Could not find a usable version of Winsock.dll\n"
            "  Expected: 2.2\n"
            "  Actual: %d.%d\n",
            (int)LOBYTE(wsa_data.wVersion),
            (int)HIBYTE(wsa_data.wVersion));
    server.ok = false;
  }

  return server;
}

static ah_server create_completion_port(ah_server server)
{
  if (!server.ok) {
    return server;
  }

  HANDLE completion_port =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
  if (completion_port == NULL) {
    print_error("CreateIoCompletionPort", GetLastError());
    server.ok = false;
  } else {
    server.completion_port = completion_port;
  }

  return server;
}

bool create_server(ah_server* result_server)
{
  ah_server server = {
      .ok = true,
      .completion_port = INVALID_HANDLE_VALUE,
  };
  server = startup(server);
  server = create_completion_port(server);

  memcpy(result_server, &server, server_size());
  return server.ok;
}

typedef struct ah_socket {
  bool ok;
  SOCKET socket;
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

  SOCKET unbound_socket = WSASocket(
      AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (unbound_socket == INVALID_SOCKET) {
    print_error("WSASocket", WSAGetLastError());
    socket.ok = false;
  } else {
    socket.socket = unbound_socket;
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
  if (bind(socket.socket, address_ptr, sizeof(address)) == SOCKET_ERROR) {
    print_error("bind", WSAGetLastError());
    socket.ok = false;
  }

  return socket;
}

static ah_socket listen_on_socket(ah_socket socket)
{
  if (!socket.ok) {
    return socket;
  }

  if (listen(socket.socket, SOMAXCONN) == SOCKET_ERROR) {
    print_error("listen", WSAGetLastError());
    socket.ok = false;
  }

  return socket;
}

bool create_socket(ah_socket* result_socket, uint16_t port)
{
  ah_socket socket = {.ok = true, .socket = INVALID_SOCKET};
  socket = create_unbound_socket(socket);
  socket = bind_socket(socket, port);
  socket = listen_on_socket(socket);

  memcpy(result_socket, &socket, socket_size());
  return socket.ok;
}
