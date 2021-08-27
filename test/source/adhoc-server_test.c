#include <lib.h>
#include <string.h>

int main(int argc, const char* argv[])
{
  (void)argc;
  (void)argv;

  library lib = create_library();

  return strcmp(lib.name, "adhoc-server") == 0 ? 0 : 1;
}
