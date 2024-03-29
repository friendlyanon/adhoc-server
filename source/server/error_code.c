#include "server/error_code.h"

static const int values[] = {
    AH_ERR_ACCESS_DENIED,
    AH_ERR_ADDRESS_FAMILY_NOT_SUPPORTED,
    AH_ERR_ADDRESS_IN_USE,
    AH_ERR_ALREADY_CONNECTED,
    AH_ERR_ALREADY_STARTED,
    AH_ERR_BROKEN_PIPE,
    AH_ERR_CONNECTION_ABORTED,
    AH_ERR_CONNECTION_REFUSED,
    AH_ERR_CONNECTION_RESET,
    AH_ERR_BAD_DESCRIPTOR,
    AH_ERR_FAULT,
    AH_ERR_HOST_UNREACHABLE,
    AH_ERR_IN_PROGRESS,
    AH_ERR_INTERRUPTED,
    AH_ERR_INVALID_ARGUMENT,
    AH_ERR_MESSAGE_SIZE,
    AH_ERR_NAME_TOO_LONG,
    AH_ERR_NETWORK_DOWN,
    AH_ERR_NETWORK_RESET,
    AH_ERR_NETWORK_UNREACHABLE,
    AH_ERR_NO_DESCRIPTORS,
    AH_ERR_NO_BUFFER_SPACE,
    AH_ERR_NO_MEMORY,
    AH_ERR_NO_PERMISSION,
    AH_ERR_NO_PROTOCOL_OPTION,
    AH_ERR_NO_SUCH_DEVICE,
    AH_ERR_NOT_CONNECTED,
    AH_ERR_NOT_SOCKET,
    AH_ERR_OPERATION_ABORTED,
    AH_ERR_OPERATION_NOT_SUPPORTED,
    AH_ERR_SHUT_DOWN,
    AH_ERR_TIMED_OUT,
    AH_ERR_TRY_AGAIN,
    AH_ERR_WOULD_BLOCK,
};

bool is_ah_error_code(int value)
{
  const int* begin = values;
  const int* const end = &values[sizeof(values) / sizeof(int)];
  do {
    if (*begin == value) {
      return true;
    }
  } while (++begin != end);

  return false;
}
