#include "server.nt.h"

#include <MSWSock.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>

#define ERROR_MESSAGE_SIZE 256

static void wide_trim_end(const wchar_t* begin, wchar_t* end)
{
  while (1) {
    --end;
    if (end < begin || iswspace(*end) == 0) {
      end[1] = L'\0';
      break;
    }
  }
}

static void print_error(const char* function, int error_code)
{
  wchar_t error_message[ERROR_MESSAGE_SIZE];
  error_message[0] = L'\0';

  DWORD result =
      FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     NULL,
                     error_code,
                     0,
                     error_message,
                     ERROR_MESSAGE_SIZE,
                     NULL);

  if (result == 0 || error_message[0] == L'\0') {
    fwprintf(stderr, L"%S: 0x%08X\n", function, (unsigned int)error_code);
  } else {
    wide_trim_end(error_message, &error_message[result]);
    fwprintf(stderr, L"%S: %s\n", function, error_message);
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
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
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

/* Socket destruction */

bool destroy_socket(ah_socket* socket)
{
  if (socket->socket == INVALID_SOCKET) {
    return true;
  }

  if (closesocket(socket->socket) == SOCKET_ERROR) {
    print_error("closesocket", WSAGetLastError());
    return false;
  }

  socket->socket = INVALID_SOCKET;
  return true;
}

/* Acceptor creation */

typedef struct ah_overlapped_base {
  OVERLAPPED overlapped;
  bool (*handler)(LPOVERLAPPED);
} ah_overlapped_base;

static ah_overlapped_base* base_from_overlapped(LPOVERLAPPED overlapped)
{
  char* base = (char*)overlapped - offsetof(ah_overlapped_base, overlapped);
  return (ah_overlapped_base*)base;
}

#define ADDRESS_LENGTH ((DWORD)(sizeof(SOCKADDR_IN) + 16))

typedef struct ah_acceptor {
  ah_overlapped_base base;
  ah_on_accept on_accept;
  ah_server* server;
  ah_socket listening_socket;
  ah_socket socket;
  uint8_t output_buffer[ADDRESS_LENGTH * 2];
} ah_acceptor;

static ah_acceptor* acceptor_from_overlapped(LPOVERLAPPED overlapped)
{
  char* acceptor =
      (char*)base_from_overlapped(overlapped) - offsetof(ah_acceptor, base);
  return (ah_acceptor*)acceptor;
}

size_t acceptor_size()
{
  return sizeof(ah_acceptor);
}

static bool do_accept(LPOVERLAPPED overlapped);

static bool accept_handler(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);

  LPSOCKADDR_IN local_address;
  int local_address_length;
  LPSOCKADDR_IN remote_address;
  int remote_address_length;
  GetAcceptExSockaddrs(acceptor->output_buffer,
                       0,
                       ADDRESS_LENGTH,
                       ADDRESS_LENGTH,
                       (LPSOCKADDR*)&local_address,
                       &local_address_length,
                       (LPSOCKADDR*)&remote_address,
                       &remote_address_length);
  uint32_t address_raw = ntohl(remote_address->sin_addr.s_addr);
  ah_ipv4_address address = {{
      address_raw >> 24 & 0xFF,
      address_raw >> 16 & 0xFF,
      address_raw >> 8 & 0xFF,
      address_raw & 0xFF,
  }};
  /* TODO: Implement I/O for the handler */
  bool result = acceptor->on_accept(address, &acceptor->socket);
  destroy_socket(&acceptor->socket);

  return result ? do_accept(overlapped) : false;
}

static bool queue_accept_operation(HANDLE completion_port,
                                   LPOVERLAPPED overlapped)
{
  BOOL result = PostQueuedCompletionStatus(completion_port, 0, 0, overlapped);
  if (result == FALSE) {
    print_error("PostQueuedCompletionStatus", GetLastError());
    return false;
  }

  return true;
}

static bool do_accept(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);
  {
    ah_socket_slot slot =
        create_unbound_socket((ah_socket_slot) {true, {INVALID_SOCKET}});
    if (!slot.ok) {
      return false;
    }
    acceptor->socket = slot.socket;
  }

  DWORD bytes_read;
  BOOL result = AcceptEx(acceptor->listening_socket.socket,
                         acceptor->socket.socket,
                         acceptor->output_buffer,
                         0,
                         ADDRESS_LENGTH,
                         ADDRESS_LENGTH,
                         &bytes_read,
                         overlapped);
  if (result == FALSE) {
    int error_code = WSAGetLastError();
    /* If the error is WSAECONNRESET, an incoming connection was indicated, but
     * was subsequently terminated by the remote peer prior to accepting the
     * call. */
    switch (error_code) {
      case WSAECONNRESET:
        destroy_socket(&acceptor->socket);
        acceptor->base.handler = do_accept;
        return queue_accept_operation(acceptor->server->completion_port,
                                      overlapped);
      case ERROR_IO_PENDING:
        break;
      default:
        print_error("AcceptEx", error_code);
        return false;
    }
  }

  acceptor->base.handler = accept_handler;
  return true;
}

bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_server* server,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept)
{
  *result_acceptor = (ah_acceptor) {
      .base = {0},
      on_accept,
      server,
      *listening_socket,
      {INVALID_SOCKET},
  };
  return do_accept(&result_acceptor->base.overlapped);
}

/* Event loop */

bool server_tick(ah_server* server)
{
  DWORD bytes_transferred;
  ULONG_PTR completion_key;
  LPOVERLAPPED overlapped;
  BOOL result = GetQueuedCompletionStatus(server->completion_port,
                                          &bytes_transferred,
                                          &completion_key,
                                          &overlapped,
                                          INFINITE);
  if (result == FALSE) {
    print_error("GetQueuedCompletionStatus", GetLastError());
    return false;
  }

  return base_from_overlapped(overlapped)->handler(overlapped);
}
