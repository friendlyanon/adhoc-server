cmake_minimum_required(VERSION 3.14)

project(
    adhoc-server
    VERSION 0.0.1
    DESCRIPTION "AdHoc Server for PSP"
    HOMEPAGE_URL "https://github.com/friendlyanon/adhoc-server"
    LANGUAGES CXX
)

# ---- Declare library ----

configure_file(cmake/version.h.in include/version.h @ONLY)

set(
    sources
    include/main.h
    source/main.cpp
)

add_executable(AdHocServer ${sources})

set_property(TARGET AdHocServer PROPERTY RUNTIME_OUTPUT_NAME "adhoc-server")

target_include_directories(
    AdHocServer
    PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_BINARY_DIR}/include"
)

target_compile_features(AdHocServer PRIVATE cxx_std_17)

find_package(blobify REQUIRED)
find_package(SQLite3 REQUIRED)
target_link_libraries(
    AdHocServer
    PRIVATE
    blobify::blobify
    SQLite::SQLite3
)

# ---- Install ----

include(CPack)
include(GNUInstallDirs)

install(
    TARGETS AdHocServer
    RUNTIME #
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT adhoc-server_Runtime
)
