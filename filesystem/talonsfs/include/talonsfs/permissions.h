#pragma once

#include <cstdint>

#include "node.h"

namespace TalonsFS {

// Permission bits stored in Node::flags.
//
// Bit layout:
//   bit 0  READ
//   bit 1  WRITE
//   bit 2  EXECUTE
//   bit 3  PERMISSIONS_INITIALIZED
//   bits 4-7  reserved (ignored when checking permissions; never
//             rejected by integrity validation)
//
// Semantics:
//   flags == 0
//       Legacy/pre-permissions node (predates this feature, or a
//       node that has never been explicitly chmod'd). Treated as
//       full READ + WRITE + EXECUTE for backward compatibility with
//       existing on-disk images, where flags was always zero.
//
//   PERMISSIONS_INITIALIZED (bit 3) set
//       Bits 0-2 are authoritative, as written — including all
//       three clear, which is a genuine "no read, no write, no
//       execute" permission state (flags == PERMISSIONS_INITIALIZED,
//       i.e. 0b1000).
//
// NodeManager::create() sets newly-created nodes to
// READ | WRITE | EXECUTE | PERMISSIONS_INITIALIZED (0b1111), so a
// node is explicit about its permissions from the moment it exists.
// Once PERMISSIONS_INITIALIZED is set it must remain set — a future
// chmod 000 clears bits 0-2 but leaves bit 3 set, producing
// flags == PERMISSIONS_INITIALIZED, not flags == 0.

constexpr uint8_t PERMISSION_READ = 1u << 0;
constexpr uint8_t PERMISSION_WRITE = 1u << 1;
constexpr uint8_t PERMISSION_EXECUTE = 1u << 2;
constexpr uint8_t PERMISSIONS_INITIALIZED = 1u << 3;

constexpr uint8_t PERMISSIONS_FULL =
    PERMISSION_READ |
    PERMISSION_WRITE |
    PERMISSION_EXECUTE |
    PERMISSIONS_INITIALIZED;

bool hasReadPermission(const Node& node);
bool hasWritePermission(const Node& node);
bool hasExecutePermission(const Node& node);

}
