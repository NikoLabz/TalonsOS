#pragma once

#include <cstdint>

namespace TalonsFS {

enum class NodeType : uint8_t {
    Free = 0,
    File = 1,
    Directory = 2
};

struct Node {
    NodeType type;

    // Permission bits (see permissions.h for bit layout, semantics,
    // and the read/write/execute accessor helpers).
    uint8_t flags;

    uint16_t reserved;

    uint64_t size;

    // Unix timestamps in seconds since 1970-01-01 00:00:00 UTC.
    // A value of 0 means the timestamp is unset/unknown.
    uint64_t createdTime;
    uint64_t modifiedTime;

    uint64_t dataStart;
    uint64_t dataBlocks;

    uint8_t reservedSpace[32];
};

}
