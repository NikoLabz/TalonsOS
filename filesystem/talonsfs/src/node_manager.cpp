#include "../include/talonsfs/time.h"
#include "../include/talonsfs/node_manager.h"
#include "../include/talonsfs/permissions.h"

namespace TalonsFS {

NodeManager::NodeManager(
    NodeAllocator& allocator,
    NodeTable& table
)
    : allocator_(allocator),
      table_(table)
{
}

int64_t NodeManager::create(NodeType type)
{
    const int64_t index = allocator_.allocate();

    if (index < 0) {
        return -1;
    }

Node node{};

node.type = type;
node.flags = PERMISSIONS_FULL;

const uint64_t now = currentUnixTime();

node.createdTime = now;
node.modifiedTime = now;

    table_.write(
        static_cast<uint64_t>(index),
        node
    );

    return index;
}

bool NodeManager::read(
    uint64_t index,
    Node& node
) const
{
    node = table_.read(index);

    if (node.type == NodeType::Free) {
        return false;
    }

    return true;
}

bool NodeManager::write(
    uint64_t index,
    const Node& node
)
{
    Node updated = node;

    updated.modifiedTime = currentUnixTime();

    table_.write(index, updated);

    return true;
}

bool NodeManager::remove(uint64_t index)
{
    allocator_.free(index);

    Node empty{};

    table_.write(index, empty);

    return true;
}

}
