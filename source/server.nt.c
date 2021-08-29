#include "server.nt.h"

#include <MSWSock.h>
#include <stdio.h>
#include <string.h>

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
    fprintf(stderr, "WSAStartup: %d\n", startup_result);
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
    fprintf(stderr, "CreateIoCompletionPort: %lu\n", GetLastError());
    server.ok = false;
  } else {
    server.completion_port = completion_port;
  }

  return server;
}

bool create_server(void* result_server)
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
