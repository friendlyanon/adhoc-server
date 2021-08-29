#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Returns the size of the buffer to be allocated for ::create_server.
 */
size_t server_size(void);

/**
 * @brief Creates the server and writes it out to the \c result_server buffer.
 */
bool create_server(void* result_server);
