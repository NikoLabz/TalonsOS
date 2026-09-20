#pragma once

#include <cstdint>

namespace TalonsFS {

constexpr uint32_t MaxFilenameLength = 64;

struct DirectoryEntry {
    uint64_t node;
    uint8_t type;
    char name[MaxFilenameLength];
};

}
