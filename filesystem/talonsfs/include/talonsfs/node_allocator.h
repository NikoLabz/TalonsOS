#pragma once

#include <cstdint>

#include "bitmap.h"
#include "node_table.h"

namespace TalonsFS {

class NodeAllocator {
public:
    NodeAllocator(
        Bitmap& bitmap,
        NodeTable& table
    );

    int64_t allocate();

    bool free(uint64_t node);

private:
    Bitmap& bitmap_;
    NodeTable& table_;
};

}
