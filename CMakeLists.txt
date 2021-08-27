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
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    adhoc-server_lib OBJECT
    source/lib.c
)

target_include_directories(
    adhoc-server_lib ${adhoc-server_warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(adhoc-server_lib PUBLIC c_std_99)

# ---- Declare executable ----

add_executable(adhoc-server_adhoc-server source/main.c)
add_executable(adhoc-server::adhoc-server ALIAS adhoc-server_adhoc-server)

set_target_properties(
    adhoc-server_adhoc-server PROPERTIES
    OUTPUT_NAME adhoc-server
    EXPORT_NAME adhoc-server
)

target_compile_features(adhoc-server_adhoc-server PRIVATE c_std_99)

target_link_libraries(adhoc-server_adhoc-server PRIVATE adhoc-server_lib)

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
