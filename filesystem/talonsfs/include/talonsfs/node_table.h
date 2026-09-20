#pragma once

#include <cstdint>

#include "node.h"

namespace TalonsFS {

class NodeTable {
public:
    NodeTable(
        uint8_t* data,
        uint64_t nodeCount
    );

    Node read(uint64_t index) const;
    void write(uint64_t index, const Node& node);

private:
    uint8_t* data_;
    uint64_t nodeCount_;
};

}
