#pragma once

#include <cstdint>

#include "node.h"
#include "node_allocator.h"
#include "node_table.h"

namespace TalonsFS {

class NodeManager {
public:
    NodeManager(
        NodeAllocator& allocator,
        NodeTable& table
    );

    int64_t create(NodeType type);

    bool read(
        uint64_t index,
        Node& node
    ) const;

    bool write(
        uint64_t index,
        const Node& node
    );

    bool remove(uint64_t index);

private:
    NodeAllocator& allocator_;
    NodeTable& table_;
};

}
