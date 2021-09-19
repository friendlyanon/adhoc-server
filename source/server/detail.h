#pragma once

#include <stddef.h>

#include "server.h"

/* clang-format off */

/**
 * @brief Determine the containing struct of \c pointer given the \c type of
 * that struct and which \c member it is pointing to.
 */
#define parentof(pointer, type, member) \
  (type*)((char*)(pointer) - offsetof(type, member))

/* clang-format on */
