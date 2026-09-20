#include "../include/talonsfs/node_allocator.h"

namespace TalonsFS {

NodeAllocator::NodeAllocator(
    Bitmap& bitmap,
    NodeTable& table
)
    : bitmap_(bitmap),
      table_(table)
{
}

int64_t NodeAllocator::allocate()
{
    for (uint64_t i = 0; i < bitmap_.size(); ++i)
    {
        if (!bitmap_.get(i))
        {
            bitmap_.set(i);
            return static_cast<int64_t>(i);
        }
    }

    return -1;
}

bool NodeAllocator::free(uint64_t node)
{
    if (node >= bitmap_.size())
    {
        return false;
    }

    bitmap_.clear(node);
    return true;
}

}
