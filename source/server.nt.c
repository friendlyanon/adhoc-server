#include "server.nt.h"

#include <MSWSock.h>
#include <assert.h>
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
                     (DWORD)error_code,
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
    print_error("CreateIoCompletionPort", (int)GetLastError());
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

typedef struct ah_overlapped_base {
  OVERLAPPED overlapped;
  bool (*handler)(LPOVERLAPPED);
} ah_overlapped_base;

static ah_overlapped_base* base_from_overlapped(LPOVERLAPPED overlapped)
{
  char* base = (char*)overlapped - offsetof(ah_overlapped_base, overlapped);
  return (ah_overlapped_base*)base;
}

typedef struct ah_socket {
  SOCKET socket;
  ah_overlapped_base base;
  void* on_complete;
} ah_socket;

static ah_socket* socket_from_overlapped(LPOVERLAPPED overlapped)
{
  char* acceptor =
      (char*)base_from_overlapped(overlapped) - offsetof(ah_socket, base);
  return (ah_socket*)acceptor;
}

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

static ah_socket_slot create_unbound_socket(ah_socket_slot slot,
                                            int* error_code)
{
  if (!slot.ok) {
    return slot;
  }

  SOCKET unbound_socket = WSASocket(
      AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (unbound_socket == INVALID_SOCKET) {
    if (error_code == NULL) {
      print_error("WSASocket", WSAGetLastError());
    } else {
      *error_code = WSAGetLastError();
    }

    slot.ok = false;
  } else {
    slot.socket.socket = unbound_socket;
  }

  return slot;
}

static ah_socket_slot register_socket(ah_socket_slot slot,
                                      ah_server* server,
                                      int* error_code)
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
    if (error_code == NULL) {
      print_error("CreateIoCompletionPort", (int)GetLastError());
    } else {
      *error_code = (int)GetLastError();
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
  ah_socket_slot slot = {true, {.socket = INVALID_SOCKET}};
  slot = create_unbound_socket(slot, NULL);
  slot = register_socket(slot, server, NULL);
  slot = socket_enable_address_reuse(slot);
  slot = bind_socket(slot, port);
  slot = listen_on_socket(slot);

  memcpy(result_socket, &slot.socket, socket_size());
  return slot.ok;
}

/* Socket destruction */

bool destroy_socket(ah_server* server, ah_socket* socket)
{
  (void)server;

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

bool destroy_socket_accepted(ah_server* server, ah_socket_accepted* socket)
{
  return destroy_socket(server, (ah_socket*)socket);
}

/* Acceptor creation */

#define ADDRESS_LENGTH ((DWORD)(sizeof(SOCKADDR_IN) + 16))

typedef struct ah_acceptor {
  ah_socket listening_socket;
  void* user_data;
  ah_server* server;
  ah_socket socket;
  uint8_t output_buffer[ADDRESS_LENGTH * 2];
} ah_acceptor;

static ah_acceptor* acceptor_from_overlapped(LPOVERLAPPED overlapped)
{
  char* acceptor = (char*)socket_from_overlapped(overlapped)
      - offsetof(ah_acceptor, listening_socket);
  return (ah_acceptor*)acceptor;
}

size_t acceptor_size()
{
  return sizeof(ah_acceptor);
}

static ah_on_accept on_accept_from_acceptor(ah_acceptor* acceptor)
{
  return (ah_on_accept)acceptor->listening_socket.on_complete;
}

static bool accept_error_handler(ah_acceptor* acceptor,
                                 const char* function,
                                 int error_code)
{
  if (is_ah_error_code(error_code)) {
    ah_on_accept on_accept = on_accept_from_acceptor(acceptor);
    ah_socket_slot slot = {0};
    return on_accept((ah_error_code)error_code,
                     acceptor->server,
                     &slot.socket,
                     (ah_ipv4_address) {0},
                     acceptor->user_data);
  }

  print_error(function, error_code);
  return false;
}

static bool do_accept(LPOVERLAPPED overlapped);

static bool accept_handler(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);
  ah_server* server = acceptor->server;
  {
    int error_code;
    ah_socket_slot slot = register_socket(
        (ah_socket_slot) {true, acceptor->socket}, server, &error_code);
    if (!slot.ok) {
      return accept_error_handler(
          acceptor, "CreateIoCompletionPort", error_code);
    }
    acceptor->socket = slot.socket;
  }

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
  ah_ipv4_address address = {
      {address_raw >> 24 & 0xFF,
       address_raw >> 16 & 0xFF,
       address_raw >> 8 & 0xFF,
       address_raw & 0xFF},
      ntohs(remote_address->sin_port),
  };
  ah_socket_slot slot = {true, acceptor->socket};
  /* TODO: Implement I/O for the handler */
  ah_on_accept on_accept = on_accept_from_acceptor(acceptor);
  bool result =
      on_accept(AH_ERR_OK, server, &slot.socket, address, acceptor->user_data);
  /* If ownership of the socket wasn't taken by the handler, then it gets
   * destroyed */
  if (slot.ok) {
    result = destroy_socket(server, &slot.socket) && result;
  }

  return result ? do_accept(overlapped) : false;
}

static bool queue_overlapped(HANDLE completion_port, LPOVERLAPPED overlapped)
{
  BOOL result = PostQueuedCompletionStatus(completion_port, 0, 0, overlapped);
  if (result == FALSE) {
    print_error("PostQueuedCompletionStatus", (int)GetLastError());
    return false;
  }

  return true;
}

static bool do_accept(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);
  {
    int error_code;
    ah_socket_slot slot = create_unbound_socket(
        (ah_socket_slot) {true, {.socket = INVALID_SOCKET}}, &error_code);
    if (!slot.ok) {
      return accept_error_handler(acceptor, "WSASocket", error_code);
    }
    acceptor->socket = slot.socket;
  }

  DWORD bytes_read;
  BOOL accept_result = AcceptEx(acceptor->listening_socket.socket,
                                acceptor->socket.socket,
                                acceptor->output_buffer,
                                0,
                                ADDRESS_LENGTH,
                                ADDRESS_LENGTH,
                                &bytes_read,
                                overlapped);
  if (accept_result == FALSE) {
    int error_code = WSAGetLastError();
    if (error_code != ERROR_IO_PENDING) {
      if (!is_ah_error_code(error_code)) {
        print_error("AcceptEx", error_code);
        return false;
      }

      ah_server* server = acceptor->server;
      ah_on_accept on_accept = on_accept_from_acceptor(acceptor);
      ah_socket_slot slot = {0};
      bool result = on_accept((ah_error_code)error_code,
                              server,
                              &slot.socket,
                              (ah_ipv4_address) {0},
                              acceptor->user_data);
      destroy_socket(server, &acceptor->socket);
      if (!result) {
        return false;
      }

      acceptor->listening_socket.base.handler = do_accept;
      return queue_overlapped(server->completion_port, overlapped);
    }
  }

  acceptor->listening_socket.base.handler = accept_handler;
  return true;
}

bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_server* server,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept,
                     void* user_data)
{
  *result_acceptor = (ah_acceptor) {
      *listening_socket,
      user_data,
      server,
      {.socket = INVALID_SOCKET},
      {0},
  };
  result_acceptor->listening_socket.on_complete = (void*)on_accept;
  return do_accept(&result_acceptor->listening_socket.base.overlapped);
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

/* Event loop */

bool server_tick(ah_server* server, int* error_code)
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
    if (error_code == NULL) {
      print_error("GetQueuedCompletionStatus", (int)GetLastError());
    } else {
      *error_code = (int)GetLastError();
    }

    return false;
  }

  assert(overlapped != NULL);
  ah_overlapped_base* base = base_from_overlapped(overlapped);
  return base->handler(overlapped);
}
