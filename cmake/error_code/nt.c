#include <stdio.h>
#include <winerror.h>

static struct error_tuple {
  const char* name;
  int value;
} tuples[] = {
    {"OK", 0},
    {"ACCESS_DENIED", WSAEACCES},
    {"ADDRESS_FAMILY_NOT_SUPPORTED", WSAEAFNOSUPPORT},
    {"ADDRESS_IN_USE", WSAEADDRINUSE},
    {"ALREADY_CONNECTED", WSAEISCONN},
    {"ALREADY_STARTED", WSAEALREADY},
    {"BROKEN_PIPE", ERROR_BROKEN_PIPE},
    {"CONNECTION_ABORTED", WSAECONNABORTED},
    {"CONNECTION_REFUSED", WSAECONNREFUSED},
    {"CONNECTION_RESET", WSAECONNRESET},
    {"BAD_DESCRIPTOR", WSAEBADF},
    {"FAULT", WSAEFAULT},
    {"HOST_UNREACHABLE", WSAEHOSTUNREACH},
    {"IN_PROGRESS", WSAEINPROGRESS},
    {"INTERRUPTED", WSAEINTR},
    {"INVALID_ARGUMENT", WSAEINVAL},
    {"MESSAGE_SIZE", WSAEMSGSIZE},
    {"NAME_TOO_LONG", WSAENAMETOOLONG},
    {"NETWORK_DOWN", WSAENETDOWN},
    {"NETWORK_RESET", WSAENETRESET},
    {"NETWORK_UNREACHABLE", WSAENETUNREACH},
    {"NO_DESCRIPTORS", WSAEMFILE},
    {"NO_BUFFER_SPACE", WSAENOBUFS},
    {"NO_MEMORY", ERROR_OUTOFMEMORY},
    {"NO_PERMISSION", ERROR_ACCESS_DENIED},
    {"NO_PROTOCOL_OPTION", WSAENOPROTOOPT},
    {"NO_SUCH_DEVICE", ERROR_BAD_UNIT},
    {"NOT_CONNECTED", WSAENOTCONN},
    {"NOT_SOCKET", WSAENOTSOCK},
    {"OPERATION_ABORTED", ERROR_OPERATION_ABORTED},
    {"OPERATION_NOT_SUPPORTED", WSAEOPNOTSUPP},
    {"SHUT_DOWN", WSAESHUTDOWN},
    {"TIMED_OUT", WSAETIMEDOUT},
    {"TRY_AGAIN", ERROR_RETRY},
    {"WOULD_BLOCK", WSAEWOULDBLOCK},
};

int main(int argc, const char* argv[])
{
  (void)argc;
  (void)argv;

  char separator[2] = {0};
  for (int i = 0; i < sizeof(tuples) / sizeof(struct error_tuple); ++i) {
    struct error_tuple tuple = tuples[i];
    printf("%s%s\\;%d", separator, tuple.name, tuple.value);
    separator[0] = ';';
  }

  return 0;
}
