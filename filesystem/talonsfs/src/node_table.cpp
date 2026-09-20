#include "../include/talonsfs/node_table.h"

namespace TalonsFS {

NodeTable::NodeTable(
    uint8_t* data,
    uint64_t nodeCount
)
    : data_(data),
      nodeCount_(nodeCount)
{
}

Node NodeTable::read(uint64_t index) const
{
    Node node{};

    if (index >= nodeCount_) {
        return node;
    }

    const uint64_t offset =
        index * sizeof(Node);

    const uint8_t* source =
        data_ + offset;

    __builtin_memcpy(
        &node,
        source,
        sizeof(Node)
    );

    return node;
}

void NodeTable::write(
    uint64_t index,
    const Node& node
)
{
    if (index >= nodeCount_) {
        return;
    }

    const uint64_t offset =
        index * sizeof(Node);

    uint8_t* destination =
        data_ + offset;

    __builtin_memcpy(
        destination,
        &node,
        sizeof(Node)
    );
}

}
