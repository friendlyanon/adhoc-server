#include <errno.h>
#include <stdio.h>

static struct error_tuple {
  const char* name;
  int value;
} tuples[] = {
    {"OK", 0},
    {"ACCESS_DENIED", EACCES},
    {"ADDRESS_FAMILY_NOT_SUPPORTED", EAFNOSUPPORT},
    {"ADDRESS_IN_USE", EADDRINUSE},
    {"ALREADY_CONNECTED", EISCONN},
    {"ALREADY_STARTED", EALREADY},
    {"BROKEN_PIPE", EPIPE},
    {"CONNECTION_ABORTED", ECONNABORTED},
    {"CONNECTION_REFUSED", ECONNREFUSED},
    {"CONNECTION_RESET", ECONNRESET},
    {"BAD_DESCRIPTOR", EBADF},
    {"FAULT", EFAULT},
    {"HOST_UNREACHABLE", EHOSTUNREACH},
    {"IN_PROGRESS", EINPROGRESS},
    {"INTERRUPTED", EINTR},
    {"INVALID_ARGUMENT", EINVAL},
    {"MESSAGE_SIZE", EMSGSIZE},
    {"NAME_TOO_LONG", ENAMETOOLONG},
    {"NETWORK_DOWN", ENETDOWN},
    {"NETWORK_RESET", ENETRESET},
    {"NETWORK_UNREACHABLE", ENETUNREACH},
    {"NO_DESCRIPTORS", EMFILE},
    {"NO_BUFFER_SPACE", ENOBUFS},
    {"NO_MEMORY", ENOMEM},
    {"NO_PERMISSION", EPERM},
    {"NO_PROTOCOL_OPTION", ENOPROTOOPT},
    {"NO_SUCH_DEVICE", ENODEV},
    {"NOT_CONNECTED", ENOTCONN},
    {"NOT_SOCKET", ENOTSOCK},
    {"OPERATION_ABORTED", ECANCELED},
    {"OPERATION_NOT_SUPPORTED", EOPNOTSUPP},
    {"SHUT_DOWN", ESHUTDOWN},
    {"TIMED_OUT", ETIMEDOUT},
    {"TRY_AGAIN", EAGAIN},
    {"WOULD_BLOCK", EWOULDBLOCK},
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
