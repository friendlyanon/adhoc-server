# This function will add shared libraries to the PATH when running the test, so
# they can be found. Windows does not support RPATH or similar. See:
# https://docs.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order
# Usage: windows_set_path(<test> <target>...)
function(windows_set_path TEST)
  if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    return()
  endif()

  set(path "")
  set(glue "")
  foreach(target IN LISTS ARGN)
    get_target_property(type "${target}" TYPE)
    if(type STREQUAL "SHARED_LIBRARY")
      set(path "${path}${glue}$<TARGET_FILE_DIR:${target}>")
      set(glue "\;") # backslash is important
    endif()
  endforeach()
  if(NOT path STREQUAL "")
    set_property(TEST "${TEST}" PROPERTY ENVIRONMENT "PATH=${path}")
  endif()
endfunction()

function(generate_error_codes source)
  if(NOT DEFINED CMAKE_TRY_COMPILE_CONFIGURATION)
    set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
  endif()
  try_run(
      ERROR_CODE_RUN_RESULT ERROR_CODE_COMPILE_RESULT
      "${PROJECT_BINARY_DIR}/generated/build/error_code" "${source}"
      RUN_OUTPUT_VARIABLE output
  )
  if(NOT ERROR_CODE_COMPILE_RESULT)
    message(FATAL_ERROR "Compiling the error code generator failed")
  endif()
  if(ERROR_CODE_RUN_RESULT STREQUAL "FAILED_TO_RUN")
    message(FATAL_ERROR "Running the error code generator failed")
  endif()

  list(JOIN output ",\n  AH_ERR_" enums)
  string(REPLACE ";" " = " enums "${enums}")
  file(
      GENERATE OUTPUT "${PROJECT_BINARY_DIR}/generated/error_code/error_code.h"
      CONTENT "\
#pragma once

typedef enum ah_error_code
{
  AH_ERR_${enums},
} ah_error_code;

/**
 * @brief Tells whether the error code is one that the client code can handle.
 */
bool is_ah_error_code(int value);
"
  )

  unset(values)
  foreach(pair IN LISTS output)
    if(NOT DEFINED values)
      # This way the OK value is skipped
      set(values "")
    else()
      list(GET pair 1 value)
      list(APPEND values "${value}")
    endif()
  endforeach()
  list(JOIN values ",\n    " values)
  file(
      GENERATE OUTPUT "${PROJECT_BINARY_DIR}/generated/error_code/error_code.c"
      CONTENT "\
#include <stdbool.h>

#include \"error_code.h\"

static const int values[] = {
    ${values},
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
"
  )
endfunction()
