// NOTE: host-only. Uses <vector>/<algorithm>/<limits> or other hosted-C++/libc
// features and cannot compile in the freestanding kernel build. Used by the
// TalonsFS test suite and host-side tools (e.g. mkTalonsFS) only — never
// #include this from kernel/ code.

#include <cstring>
#include <vector>

#include "../include/talonsfs/formatter.h"
#include "../include/talonsfs/layout.h"
#include "../include/talonsfs/node.h"
#include "../include/talonsfs/superblock_utils.h"

namespace TalonsFS {
namespace {

constexpr uint64_t SuperblockMagic =
    0x54414C4F4E534653ULL;

constexpr uint32_t FilesystemVersion = 1;

void setBit(
    std::vector<uint8_t>& bitmap,
    uint64_t index
)
{
    const uint64_t byteIndex = index / 8;
    const uint8_t bitIndex =
        static_cast<uint8_t>(index % 8);

    bitmap[byteIndex] |=
        static_cast<uint8_t>(1u << bitIndex);
}

}

Superblock createSuperblock(BlockDevice& device)
{
    const FilesystemLayout layout =
        calculateLayout(
            device.getBlockCount(),
            device.getBlockSize()
        );

    Superblock sb{};

    sb.magic = SuperblockMagic;
    sb.version = FilesystemVersion;

    sb.blockSize = device.getBlockSize();
    sb.totalBlocks = device.getBlockCount();

    sb.blockBitmapStart =
        layout.blockBitmapStart;

    sb.blockBitmapBlocks =
        layout.blockBitmapBlocks;

    sb.nodeBitmapStart =
        layout.nodeBitmapStart;

    sb.nodeBitmapBlocks =
        layout.nodeBitmapBlocks;

    sb.nodeTableStart =
        layout.nodeTableStart;

    sb.nodeTableBlocks =
        layout.nodeTableBlocks;

    sb.nodeCount =
        layout.nodeCount;

	sb.dataStart =
    layout.dataStart;

sb.rootNode =
    RootNode;

sb.filesystemID =
    0x54414C4F4E534653ULL;

sb.checksum =
    calculateSuperblockChecksum(
        sb
    );

return sb;

}

bool format(
    BlockDevice& device,
    Superblock& superblock
)
{
    const uint32_t blockSize =
        device.getBlockSize();

    const uint64_t totalBlocks =
        device.getBlockCount();

    if (blockSize == 0 ||
        totalBlocks < 2)
    {
        return false;
    }

    const FilesystemLayout layout =
        calculateLayout(
            totalBlocks,
            blockSize
        );

    if (layout.nodeCount == 0 ||
        layout.dataStart >= totalBlocks)
    {
        return false;
    }

    superblock =
        createSuperblock(device);

   const uint64_t dataBlockCount =
    totalBlocks - layout.dataStart;

superblock.freeBlocks =
    dataBlockCount;

superblock.freeNodes =
    layout.nodeCount - 1;

superblock.checksum =
    calculateSuperblockChecksum(
        superblock
    );

    /*
     * -------------------------
     * Block bitmap
     * -------------------------
     *
     * Every block before dataStart
     * belongs to filesystem metadata
     * and must therefore be marked used.
     */

    const uint64_t blockBitmapBytes =
        (totalBlocks + 7) / 8;

    std::vector<uint8_t> blockBitmap(
        blockBitmapBytes,
        0
    );

    for (uint64_t block = 0;
         block < layout.dataStart;
         ++block)
    {
        setBit(blockBitmap, block);
    }

    for (uint64_t i = 0;
         i < layout.blockBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> blockBuffer(
            blockSize,
            0
        );

        const uint64_t offset =
            i * blockSize;

        const uint64_t remaining =
            blockBitmapBytes - offset;

        const uint64_t bytesToCopy =
            remaining < blockSize
                ? remaining
                : blockSize;

        std::memcpy(
            blockBuffer.data(),
            blockBitmap.data() + offset,
            bytesToCopy
        );

        if (!device.writeBlock(
                layout.blockBitmapStart + i,
                blockBuffer.data()
            ))
        {
            return false;
        }
    }

    /*
     * -------------------------
     * Node bitmap
     * -------------------------
     *
     * Node 0 is the root directory,
     * so it is reserved immediately.
     */

    const uint64_t nodeBitmapBytes =
        (layout.nodeCount + 7) / 8;

    std::vector<uint8_t> nodeBitmap(
        nodeBitmapBytes,
        0
    );

    setBit(
        nodeBitmap,
        RootNode
    );

    for (uint64_t i = 0;
         i < layout.nodeBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> blockBuffer(
            blockSize,
            0
        );

        const uint64_t offset =
            i * blockSize;

        const uint64_t remaining =
            nodeBitmapBytes - offset;

        const uint64_t bytesToCopy =
            remaining < blockSize
                ? remaining
                : blockSize;

        std::memcpy(
            blockBuffer.data(),
            nodeBitmap.data() + offset,
            bytesToCopy
        );

        if (!device.writeBlock(
                layout.nodeBitmapStart + i,
                blockBuffer.data()
            ))
        {
            return false;
        }
    }

    /*
     * -------------------------
     * Node table
     * -------------------------
     *
     * Start completely empty.
     * Then write Node 0 as "/".
     */

    std::vector<uint8_t> emptyBlock(
        blockSize,
        0
    );

    for (uint64_t i = 0;
         i < layout.nodeTableBlocks;
         ++i)
    {
        if (!device.writeBlock(
                layout.nodeTableStart + i,
                emptyBlock.data()
            ))
        {
            return false;
        }
    }

    Node root{};
    root.type = NodeType::Directory;

    std::vector<uint8_t> firstNodeBlock(
        blockSize,
        0
    );

    std::memcpy(
        firstNodeBlock.data(),
        &root,
        sizeof(Node)
    );

    if (!device.writeBlock(
            layout.nodeTableStart,
            firstNodeBlock.data()
        ))
    {
        return false;
    }

    /*
     * -------------------------
     * Superblock
     * -------------------------
     */

    std::vector<uint8_t> superblockBuffer(
        blockSize,
        0
    );

    std::memcpy(
        superblockBuffer.data(),
        &superblock,
        sizeof(Superblock)
    );

    if (!device.writeBlock(
            layout.superblockBlock,
            superblockBuffer.data()
        ))
    {
        return false;
    }

    return true;
}

}
