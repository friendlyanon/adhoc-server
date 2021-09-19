#pragma once

#include <MSWSock.h>

#include "server/detail.h"

/**
 * @file
 *
 * This header is a proxy header, whose only purpose is to include
 * <tt>&lt;MSWSock&gt;</tt> after everything else is included in
 * <tt>server.nt.c</tt>. This is necessary, because <tt>&lt;MSWSock.h&gt;</tt>
 * does not include headers on which it depends.
 */
