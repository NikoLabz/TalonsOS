#include "../include/talonsfs/layout.h"
#include "../include/talonsfs/node.h"

namespace TalonsFS {

FilesystemLayout calculateLayout(
    uint64_t totalBlocks,
    uint32_t blockSize
)
{
    FilesystemLayout layout{};

    layout.superblockBlock = 1;

    uint64_t blockBitmapBytes =
        (totalBlocks + 7) / 8;

    layout.blockBitmapStart = 2;

    layout.blockBitmapBlocks =
        (blockBitmapBytes + blockSize - 1) / blockSize;

    layout.nodeBitmapStart =
        layout.blockBitmapStart +
        layout.blockBitmapBlocks;

    constexpr uint64_t bytesPerNode = 16 * 1024;

    uint64_t totalBytes =
        totalBlocks * blockSize;

    layout.nodeCount =
        totalBytes / bytesPerNode;

    uint64_t bitsPerBlock =
        static_cast<uint64_t>(blockSize) * 8;

    layout.nodeBitmapBlocks =
        (layout.nodeCount + bitsPerBlock - 1) /
        bitsPerBlock;

    layout.nodeTableStart =
        layout.nodeBitmapStart +
        layout.nodeBitmapBlocks;

    uint64_t nodeTableBytes =
        layout.nodeCount * sizeof(Node);

    layout.nodeTableBlocks =
        (nodeTableBytes + blockSize - 1) / blockSize;

    layout.dataStart =
        layout.nodeTableStart +
        layout.nodeTableBlocks;

    return layout;
}

}
