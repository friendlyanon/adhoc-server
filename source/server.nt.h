#pragma once

#include <WinSock2.h>

#include "server.h"

/**
 * This header is a proxy header, whose only purpose is to include
 * <tt>&lt;WinSock2&gt;</tt> before anything else is included in
 * <tt>server.nt.c</tt>. This is necessary, because <tt>&lt;MSWSock.h&gt;</tt>
 * does not include headers on which it depends.
 */
