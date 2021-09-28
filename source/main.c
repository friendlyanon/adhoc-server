#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif
#include <stdio.h>

#include "lib.h"

int main(int argc, const char* argv[])
{
  (void)argc;
  (void)argv;

#ifdef _WIN32
  /* The library will report errors from the TCP server to stderr, but the
   * messages are retrieved using FormatMessageW and if the system language
   * isn't english, then there is a possibility that the user will see garbled
   * text instead of the cause of the error */
  _setmode(_fileno(stderr), _O_U16TEXT);
#endif

  library lib = create_library();
  printf("Hello from %s!", lib.name);
  return 0;
}
