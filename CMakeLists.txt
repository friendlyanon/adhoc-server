cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    adhoc-server
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES C
)

docs_early_return()

include(cmake/project-is-top-level.cmake)
include(cmake/functions.cmake)
include(cmake/variables.cmake)

# ---- Declare libraries ----

add_library(adhoc-server_server OBJECT source/server/error_code.c)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(adhoc-server_server PRIVATE source/server/nt.c)
  target_link_libraries(adhoc-server_server PRIVATE Mswsock Ws2_32)
  target_compile_definitions(
      adhoc-server_server PRIVATE
      WIN32_LEAN_AND_MEAN
      UNICODE
  )
  if(NOT CMAKE_VERSION VERSION_LESS "3.16")
    target_precompile_headers(
        adhoc-server_server PRIVATE
        <WinSock2.h> # this must come before MSWSock.h
        <MSWSock.h>
    )
  endif()
  set(ah_socket_accepted_size 16)
  set(ah_io_operation_size 72)
  set(ah_error_code_category nt)
else()
  target_sources(adhoc-server_server PRIVATE source/server/posix.c)
  set(ah_socket_accepted_size 16)
  set(ah_io_operation_size 32)
  set(ah_error_code_category posix)
endif()

set(
    adhoc-server_AH_SOCKET_ACCEPTED_SIZE "${ah_socket_accepted_size}"
    CACHE STRING "The size of the ah_socket_accepted struct"
)
set(
    adhoc-server_AH_SOCKET_ACCEPTED_ALIGNMENT "${CMAKE_SIZEOF_VOID_P}"
    CACHE STRING "The alignment of the ah_socket_accepted struct"
)
set(
    adhoc-server_AH_IO_OPERATION_SIZE "${ah_io_operation_size}"
    CACHE STRING "The size of the ah_io_operation struct"
)
set(
    adhoc-server_AH_IO_OPERATION_ALIGNMENT "${CMAKE_SIZEOF_VOID_P}"
    CACHE STRING "The alignment of the ah_io_operation struct"
)
target_compile_definitions(
    adhoc-server_server PUBLIC
    "AH_SOCKET_ACCEPTED_SIZE=${adhoc-server_AH_SOCKET_ACCEPTED_SIZE}"
    "AH_SOCKET_ACCEPTED_ALIGNMENT=${adhoc-server_AH_SOCKET_ACCEPTED_ALIGNMENT}"
    "AH_IO_OPERATION_SIZE=${adhoc-server_AH_IO_OPERATION_SIZE}"
    "AH_IO_OPERATION_ALIGNMENT=${adhoc-server_AH_IO_OPERATION_ALIGNMENT}"
)

set(
    adhoc-server_ERROR_CODE_GENERATOR
    "${PROJECT_SOURCE_DIR}/cmake/error_code/${ah_error_code_category}.c"
    CACHE FILEPATH
    "The C file to compile and run that generates a list of error code pairs"
)
generate_error_codes("${adhoc-server_ERROR_CODE_GENERATOR}")

target_include_directories(
    adhoc-server_server ${adhoc-server_warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated/error_code>"
)

target_compile_features(adhoc-server_server PUBLIC c_std_11)

add_library(
    adhoc-server_lib OBJECT
    source/lib.c
)

target_include_directories(
    adhoc-server_lib ${adhoc-server_warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(adhoc-server_lib PUBLIC c_std_11)

target_link_libraries(adhoc-server_lib PRIVATE adhoc-server_server)

# ---- Declare executable ----

add_executable(adhoc-server_adhoc-server source/main.c)
add_executable(adhoc-server::adhoc-server ALIAS adhoc-server_adhoc-server)

set_target_properties(
    adhoc-server_adhoc-server PROPERTIES
    OUTPUT_NAME adhoc-server
    EXPORT_NAME adhoc-server
)

target_compile_features(adhoc-server_adhoc-server PRIVATE c_std_11)

target_link_libraries(
    adhoc-server_adhoc-server PRIVATE
    adhoc-server_server
    adhoc-server_lib
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT adhoc-server_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of adhoc-server"
  )
endif()

include(cmake/dev-mode.cmake)
