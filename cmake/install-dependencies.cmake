cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED ENV{INSTALL_PREFIX})
  message(FATAL_ERROR "No INSTALL_PREFIX env variable provided")
endif()

get_filename_component(prefix "$ENV{INSTALL_PREFIX}" REALPATH)
if(NOT DEFINED ENV{__installing})
  set(ENV{__installing} YES)
  if(IS_DIRECTORY "${prefix}")
    return()
  endif()
endif()

set(ENV{INSTALL_PREFIX} "${prefix}")

if(NOT DEFINED ENV{BUILD_TYPE})
  message(STATUS "No BUILD_TYPE env variable provided, using Release")
  set(build Release)
  set(ENV{BUILD_TYPE} Release)
else()
  set(build "$ENV{BUILD_TYPE}")
endif()

if(NOT DEFINED ENV{__git_path})
  find_package(Git REQUIRED)
  set(ENV{__git_path} "${GIT_EXECUTABLE}")
else()
  set(GIT_EXECUTABLE "$ENV{__git_path}")
endif()

function(call)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE result)
  if(NOT result STREQUAL 0)
    string(REPLACE ";" " " command "${ARGV}")
    message(FATAL_ERROR "Command exited with ${result}: ${command}")
  endif()
endfunction()

function(cmake)
  call("${CMAKE_COMMAND}" ${ARGV})
endfunction()

function(git)
  call("${GIT_EXECUTABLE}" ${ARGV})
endfunction()

include(ProcessorCount)
ProcessorCount(N)

set(deps "https://github.com/friendlyanon/blobify;master")

set(cmake_vars "-DCMAKE_BUILD_TYPE=${build}" "-DCMAKE_INSTALL_PREFIX=${prefix}")

while(1)
  list(LENGTH deps length)
  if(length STREQUAL 0)
    return()
  endif()
  list(GET deps 0 repo)
  list(REMOVE_AT deps 0)
  list(GET deps 0 branch)
  list(REMOVE_AT deps 0)

  git(clone "${repo}" --branch "${branch}" --single-branch _clone)
  if(EXISTS "_clone/cmake/install-dependencies.cmake")
    cmake(-P cmake/install-dependencies.cmake WORKING_DIRECTORY _clone)
  endif()

  cmake(-S _clone -B _build ${cmake_vars})
  cmake(--build _build --config "${build}" -j "${N}")
  cmake(--install _build --config "${build}")

  cmake(-E remove_directory _clone)
  cmake(-E remove_directory _build)
endwhile()
