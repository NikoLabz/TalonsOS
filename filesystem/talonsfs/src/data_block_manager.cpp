#include "../include/talonsfs/data_block_manager.h"

namespace TalonsFS {

DataBlockManager::DataBlockManager(
    BlockDevice& device
)
    : device_(device)
{
}

bool DataBlockManager::read(
    uint64_t blockNumber,
    void* buffer
)
{
    return device_.readBlock(
        blockNumber,
        buffer
    );
}

bool DataBlockManager::write(
    uint64_t blockNumber,
    const void* buffer
)
{
    return device_.writeBlock(
        blockNumber,
        buffer
    );
}

uint32_t DataBlockManager::getBlockSize() const
{
    return device_.getBlockSize();
}

}
