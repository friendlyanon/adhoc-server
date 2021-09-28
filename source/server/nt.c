#include <WinSock2.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>

#include "server/detail.nt.h"

#define ERROR_MESSAGE_SIZE 256

static void wide_trim_end(const wchar_t* begin, wchar_t* end)
{
  assert(begin != end);

  while (1) {
    if (--end == begin || iswspace(*end) == 0) {
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
    fwprintf(stderr,
             L"%S (0x%08X): %s\n",
             function,
             (unsigned int)error_code,
             error_message);
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
  return parentof(overlapped, ah_overlapped_base, overlapped);
}

static void clear_overlapped(LPOVERLAPPED overlapped)
{
  *overlapped = (OVERLAPPED) {0};
}

typedef struct ah_socket {
  SOCKET socket;
  ah_context* context;
} ah_socket;

_Static_assert(sizeof(ah_socket) == sizeof(ah_socket_accepted),
               "AH_SOCKET_ACCEPTED_SIZE does not match the internal size");

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
                                      ah_context* context,
                                      int* error_code)
{
  if (!slot.ok) {
    return slot;
  }

  _Static_assert(sizeof(HANDLE) == sizeof(SOCKET),
                 "HANDLE and SOCKET differ in size");
  HANDLE socket_handle = NULL;
  memcpy(&socket_handle, &slot.socket.socket, sizeof(SOCKET));

  HANDLE completion_port = context->server->completion_port;
  HANDLE result = CreateIoCompletionPort(socket_handle, completion_port, 0, 0);
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

static ah_socket make_socket(ah_context* context)
{
  return (ah_socket) {INVALID_SOCKET, context};
}

bool create_socket(ah_socket* result_socket, ah_context* context, uint16_t port)
{
  ah_socket_slot slot = {true, make_socket(context)};
  slot = create_unbound_socket(slot, NULL);
  slot = register_socket(slot, context, NULL);
  slot = socket_enable_address_reuse(slot);
  slot = bind_socket(slot, port);
  slot = listen_on_socket(slot);

  memcpy(result_socket, &slot.socket, socket_size());
  return slot.ok;
}

/* Server destruction */

bool destroy_server(ah_server* server)
{
  bool result = true;

  ah_socket_span span = server->socket_span;
  for (size_t i = 0, size = span.size; i != size; ++i) {
    result = destroy_socket(&span.sockets[i]) && result;
  }

  if (server->completion_port != INVALID_HANDLE_VALUE
      && CloseHandle(server->completion_port) == 0)
  {
    print_error("CloseHandle", (int)GetLastError());
    result = false;
  }

  if (server->server_started && WSACleanup() == SOCKET_ERROR) {
    print_error("WSACleanup", WSAGetLastError());
    result = false;
  }

  server->server_started = false;
  server->completion_port = INVALID_HANDLE_VALUE;
  return result;
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

bool destroy_socket_accepted(ah_socket_accepted* socket)
{
  return destroy_socket_base((ah_socket*)socket);
}

/* Acceptor creation */

#define ADDRESS_LENGTH ((DWORD)(sizeof(SOCKADDR_IN) + 16))

typedef struct ah_acceptor {
  ah_overlapped_base base;
  ah_socket listening_socket;
  ah_on_accept on_accept;
  ah_socket socket;
  uint8_t output_buffer[ADDRESS_LENGTH * 2];
} ah_acceptor;

static ah_acceptor* acceptor_from_overlapped(LPOVERLAPPED overlapped)
{
  return parentof(base_from_overlapped(overlapped), ah_acceptor, base);
}

size_t acceptor_size()
{
  return sizeof(ah_acceptor);
}

static bool accept_on_error(ah_acceptor* acceptor, int error_code)
{
  ah_context* context = acceptor->listening_socket.context;
  ah_socket_slot slot = {false, make_socket(context)};
  return acceptor->on_accept(
      (ah_error_code)error_code, &slot.socket, (ah_ipv4_address) {0});
}

static bool accept_error_handler(ah_acceptor* acceptor,
                                 const char* function,
                                 int error_code)
{
  if (is_ah_error_code(error_code)) {
    return accept_on_error(acceptor, error_code);
  }

  print_error(function, error_code);
  return false;
}

static bool do_accept(LPOVERLAPPED overlapped);

static bool accept_handler(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);
  {
    int error_code = (int)overlapped->Offset;
    if (error_code != 0) {
      bool result = destroy_socket(&acceptor->socket);
      result = accept_on_error(acceptor, error_code) && result;
      return result;
    }
  }

  {
    int error_code;
    ah_socket_slot slot =
        register_socket((ah_socket_slot) {true, acceptor->socket},
                        acceptor->listening_socket.context,
                        &error_code);
    if (!slot.ok) {
      bool result = destroy_socket(&slot.socket);
      result =
          accept_error_handler(acceptor, "CreateIoCompletionPort", error_code)
          && result;
      return result;
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
  bool result = acceptor->on_accept(AH_ERR_OK, &slot.socket, address);
  /* If ownership of the socket wasn't taken by the handler, then it gets
   * destroyed */
  if (slot.ok) {
    result = destroy_socket(&slot.socket) && result;
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

static int map_error_code(int error_code);

static bool do_accept(LPOVERLAPPED overlapped)
{
  ah_acceptor* acceptor = acceptor_from_overlapped(overlapped);
  {
    int error_code = (int)overlapped->Offset;
    if (error_code != 0) {
      return accept_on_error(acceptor, error_code);
    }
  }

  ah_context* context = acceptor->listening_socket.context;
  {
    int error_code;
    ah_socket_slot slot = create_unbound_socket(
        (ah_socket_slot) {true, make_socket(context)}, &error_code);
    if (!slot.ok) {
      bool result = destroy_socket(&slot.socket);
      result =
          accept_error_handler(acceptor, "WSASocket", error_code) && result;
      return result;
    }
    acceptor->socket = slot.socket;
  }

  clear_overlapped(overlapped);
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
    int error_code = map_error_code(WSAGetLastError());
    if (error_code != ERROR_IO_PENDING) {
      if (!is_ah_error_code(error_code)) {
        print_error("AcceptEx", error_code);
        destroy_socket(&acceptor->socket);
        return false;
      }

      ah_socket_slot slot = {false, make_socket(context)};
      bool result = acceptor->on_accept(
          (ah_error_code)error_code, &slot.socket, (ah_ipv4_address) {0});
      result = destroy_socket(&acceptor->socket) && result;
      if (!result) {
        return false;
      }

      acceptor->base.handler = do_accept;
      return queue_overlapped(context->server->completion_port, overlapped);
    }
  }

  acceptor->base.handler = accept_handler;
  return true;
}

bool create_acceptor(ah_acceptor* result_acceptor,
                     ah_socket* listening_socket,
                     ah_on_accept on_accept)
{
  *result_acceptor = (ah_acceptor) {.base = {0}, *listening_socket, on_accept};
  return do_accept(&result_acceptor->base.overlapped);
}

/* I/O */

void move_socket(ah_socket_accepted* result_socket, ah_socket* socket)
{
  ah_socket_slot* slot = parentof(socket, ah_socket_slot, socket);
  if (slot->ok) {
    memcpy(result_socket, socket, socket_size());
    slot->ok = false;
  }
}

typedef struct ah_io_port {
  bool active;
  bool is_read_port;
  uint32_t buffer_length;
  void* buffer;
  ah_on_io_complete on_complete;
  ah_overlapped_base base;
} ah_io_port;

_Static_assert(sizeof(ah_io_port) == sizeof(ah_io_operation),
               "AH_IO_OPERATION_SIZE does not match the internal size");

bool is_io_operation_active(ah_io_operation* operation)
{
  return ((ah_io_port*)operation)->active;
}

ah_io_buffer buffer_from_io_operation(ah_io_operation* operation)
{
  ah_io_port* port = (ah_io_port*)operation;
  return (ah_io_buffer) {port->buffer_length, port->buffer};
}

ah_io_dock* dock_from_operation(ah_io_operation* operation)
{
  ah_io_port* port = (ah_io_port*)operation;
  return port->is_read_port ? parentof(port, ah_io_dock, read_port)
                            : parentof(port, ah_io_dock, write_port);
}

static ah_io_port* port_from_overlapped(LPOVERLAPPED overlapped)
{
  return parentof(base_from_overlapped(overlapped), ah_io_port, base);
}

static bool io_handler(LPOVERLAPPED overlapped)
{
  ah_io_port* port = port_from_overlapped(overlapped);
  uint32_t bytes_transferred = overlapped->OffsetHigh;
  ah_error_code error_code = (ah_error_code)(int)overlapped->Offset;

  port->active = false;
  return port->on_complete(
      error_code, (ah_io_operation*)port, bytes_transferred);
}

static void init_io_port(ah_io_port* port,
                         bool is_read_port,
                         ah_io_buffer buffer,
                         ah_on_io_complete on_complete)
{
  ah_io_port new_port = {
      .active = true,
      is_read_port,
      buffer.buffer_length,
      buffer.buffer,
      on_complete,
      .base = {.handler = io_handler},
  };
  memcpy(port, &new_port, sizeof(ah_io_port));
}

bool queue_read_operation(ah_io_dock* dock,
                          ah_io_buffer buffer,
                          ah_on_io_complete on_complete)
{
  ah_io_port* port = (ah_io_port*)&dock->read_port;
  if (buffer.buffer_length > (uint32_t)INT32_MAX || port->active) {
    return false;
  }

  init_io_port(port, true, buffer, on_complete);

  ah_socket* socket = (ah_socket*)dock->socket;
  LPOVERLAPPED overlapped = &port->base.overlapped;
  WSABUF wsa_buffer = {buffer.buffer_length, buffer.buffer};
  DWORD flags = 0;
  int result =
      WSARecv(socket->socket, &wsa_buffer, 1, NULL, &flags, overlapped, NULL);
  if (result == SOCKET_ERROR) {
    int error_code = map_error_code(WSAGetLastError());
    if (error_code != WSA_IO_PENDING) {
      if (is_ah_error_code(error_code)) {
        port->active = false;
        return on_complete(
            (ah_error_code)error_code, (ah_io_operation*)port, 0);
      }

      print_error("WSARecv", error_code);
      return false;
    }
  }

  return true;
}

bool queue_write_operation(ah_io_dock* dock,
                           ah_io_buffer buffer,
                           ah_on_io_complete on_complete)
{
  ah_io_port* port = (ah_io_port*)&dock->write_port;
  if (buffer.buffer_length > (uint32_t)INT32_MAX || port->active) {
    return false;
  }

  init_io_port(port, false, buffer, on_complete);

  ah_socket* socket = (ah_socket*)dock->socket;
  LPOVERLAPPED overlapped = &port->base.overlapped;
  WSABUF wsa_buffer = {buffer.buffer_length, buffer.buffer};
  int result =
      WSASend(socket->socket, &wsa_buffer, 1, NULL, 0, overlapped, NULL);
  if (result == SOCKET_ERROR) {
    int error_code = map_error_code(WSAGetLastError());
    if (error_code != WSA_IO_PENDING) {
      if (is_ah_error_code(error_code)) {
        port->active = false;
        return on_complete(
            (ah_error_code)error_code, (ah_io_operation*)port, 0);
      }

      print_error("WSASend", error_code);
      return false;
    }
  }

  return true;
}

/* Event loop */

static int map_error_code(int error_code)
{
  switch (error_code) {
    case ERROR_NETNAME_DELETED:
      return (int)AH_ERR_CONNECTION_RESET;
    case ERROR_PORT_UNREACHABLE:
      return (int)AH_ERR_CONNECTION_REFUSED;
  }

  return error_code;
}

bool server_tick(ah_server* server, int* error_code_out)
{
  DWORD bytes_transferred;
  ULONG_PTR completion_key;
  LPOVERLAPPED overlapped;
  BOOL result = GetQueuedCompletionStatus(server->completion_port,
                                          &bytes_transferred,
                                          &completion_key,
                                          &overlapped,
                                          INFINITE);
  assert(overlapped != NULL);
  ah_overlapped_base* base = base_from_overlapped(overlapped);
  overlapped->OffsetHigh = bytes_transferred;
  overlapped->Offset = 0;

  if (result == FALSE) {
    int error_code = map_error_code((int)GetLastError());
    if (!is_ah_error_code(error_code)) {
      if (error_code_out == NULL) {
        print_error("GetQueuedCompletionStatus", error_code);
      } else {
        *error_code_out = error_code;
      }

      return false;
    }

    overlapped->Offset = (DWORD)error_code;
  }

  return base->handler(overlapped);
}
