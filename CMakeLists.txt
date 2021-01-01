cmake_minimum_required(VERSION 3.14)

project(adhoc-server
        VERSION 0.0.1
        DESCRIPTION "AdHoc Server for PSP"
        HOMEPAGE_URL "https://github.com/friendlyanon/adhoc-server"
        LANGUAGES CXX)

# ---- Declare library ----

configure_file(cmake/version.h.in include/version.h @ONLY)

set(sources
        include/main.h
        source/main.cpp)

add_executable(AdHocServer ${sources})

set_property(TARGET AdHocServer PROPERTY RUNTIME_OUTPUT_NAME "adhoc-server")

target_include_directories(AdHocServer PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/include")

target_compile_features(AdHocServer PRIVATE cxx_std_17)

find_package(blobify REQUIRED)
target_link_libraries(AdHocServer PRIVATE blobify::blobify)

# ---- Install ----

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(TARGETS AdHocServer
        EXPORT AdHocServerTargets
        RUNTIME #
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT adhoc-server_Runtime)

write_basic_package_version_file(
        adhoc-server-config-version.cmake
        COMPATIBILITY SameMajorVersion)

set(adhoc-server_install_cmakedir
        "${CMAKE_INSTALL_LIBDIR}/cmake/adhoc-server-${PROJECT_VERSION}")

install(FILES "${PROJECT_BINARY_DIR}/adhoc-server-config-version.cmake"
        DESTINATION "${adhoc-server_install_cmakedir}"
        COMPONENT adhoc-server_Package)

install(EXPORT AdHocServerTargets
        FILE adhoc-server-config.cmake
        DESTINATION "${adhoc-server_install_cmakedir}"
        COMPONENT adhoc-server_Package)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CPack)
endif()
