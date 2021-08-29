#include "server.nt.h"

#include <MSWSock.h>
#include <stdio.h>
#include <string.h>

typedef struct server {
  bool ok;
  bool server_started;
  HANDLE completion_port;
} server;

size_t server_size()
{
  return sizeof(server);
}

static server startup(server server)
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

static server create_completion_port(server server)
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
  server server = (struct server) {
      .ok = true,
      .completion_port = INVALID_HANDLE_VALUE,
  };
  server = startup(server);
  server = create_completion_port(server);

  memcpy(result_server, &server, server_size());
  return server.ok;
}
