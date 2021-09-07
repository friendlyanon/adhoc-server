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

  set(error_code_content "#pragma once\n\ntypedef enum ah_error_code\n{\n")
  foreach(pair IN LISTS output)
    string(REPLACE ";" " = " pair "${pair}")
    string(APPEND error_code_content "  AH_ERR_${pair},\n")
  endforeach()
  file(
      GENERATE OUTPUT "${PROJECT_BINARY_DIR}/generated/error_code/error_code.h"
      CONTENT "${error_code_content}} ah_error_code;\n"
  )
endfunction()
