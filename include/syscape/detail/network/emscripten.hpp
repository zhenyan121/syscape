#ifndef SYSCAPE_DETAIL_NETWORK_EMSCRIPTEN_HPP
#define SYSCAPE_DETAIL_NETWORK_EMSCRIPTEN_HPP

// Emscripten execution instances are sandboxed and do not expose host
// network interfaces, routing tables, or hardware MAC addresses.
#include <syscape/detail/network/generic.hpp>

#endif
