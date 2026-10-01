#include "talonsfs/superblock_utils.h"
#include "filesystem.h"
#include "file_descriptor.h"
#include "block_device.h"
#include "block_cache.h"
#include "pipe_manager.h"

#include <new>
#include <stdint.h>

#include "talonsfs/constants.h"
#include "talonsfs/layout.h"
#include "talonsfs/node.h"
#include "talonsfs/superblock.h"
#include "talonsfs/directory_entry.h"
#include "talonsfs/bitmap.h"
#include "talonsfs/block_allocator.h"
#include "talonsfs/node_table.h"
#include "talonsfs/node_allocator.h"
#include "talonsfs/node_manager.h"
#include "talonsfs/node_data_manager.h"
#include "talonsfs/data_block_manager.h"
#include "talonsfs/integrity.h"

namespace
{
    constexpr uint32_t BLOCK_SIZE = 512;

    constexpr uint64_t NODE_TABLE_CAPACITY_BLOCKS = 200;
    constexpr uint64_t BLOCK_BITMAP_CAPACITY_BLOCKS = 16;
    constexpr uint64_t NODE_BITMAP_CAPACITY_BLOCKS = 4;

    KernelBlockDevice physicalDevice;
    BlockCache device(physicalDevice);

    TalonsFS::Superblock superblock{};
    TalonsFS::FilesystemLayout layout{};

    uint8_t nodeTableBuffer[
        NODE_TABLE_CAPACITY_BLOCKS * BLOCK_SIZE
    ];

    uint8_t blockBitmapBuffer[
        BLOCK_BITMAP_CAPACITY_BLOCKS * BLOCK_SIZE
    ];

    uint8_t nodeBitmapBuffer[
        NODE_BITMAP_CAPACITY_BLOCKS * BLOCK_SIZE
    ];

    uint8_t recoveryClaimedBlocks[
        BLOCK_BITMAP_CAPACITY_BLOCKS * BLOCK_SIZE
    ];

    alignas(TalonsFS::Bitmap)
        uint8_t blockBitmapStorage[
            sizeof(TalonsFS::Bitmap)
        ];

    alignas(TalonsFS::Bitmap)
        uint8_t nodeBitmapStorage[
            sizeof(TalonsFS::Bitmap)
        ];

    alignas(TalonsFS::NodeTable)
        uint8_t nodeTableStorage[
            sizeof(TalonsFS::NodeTable)
        ];

    alignas(TalonsFS::BlockAllocator)
        uint8_t blockAllocatorStorage[
            sizeof(TalonsFS::BlockAllocator)
        ];

    alignas(TalonsFS::NodeAllocator)
        uint8_t nodeAllocatorStorage[
            sizeof(TalonsFS::NodeAllocator)
        ];

    alignas(TalonsFS::NodeManager)
        uint8_t nodeManagerStorage[
            sizeof(TalonsFS::NodeManager)
        ];

    alignas(TalonsFS::NodeDataManager)
        uint8_t nodeDataManagerStorage[
            sizeof(TalonsFS::NodeDataManager)
        ];

    alignas(TalonsFS::DataBlockManager)
        uint8_t dataBlockManagerStorage[
            sizeof(TalonsFS::DataBlockManager)
        ];

    TalonsFS::Bitmap* blockBitmap = nullptr;
    TalonsFS::Bitmap* nodeBitmap = nullptr;
    TalonsFS::NodeTable* nodeTable = nullptr;
    TalonsFS::BlockAllocator* blockAllocator = nullptr;
    TalonsFS::NodeAllocator* nodeAllocator = nullptr;
    TalonsFS::NodeManager* nodeManager = nullptr;
    TalonsFS::NodeDataManager* nodeDataManager = nullptr;
    TalonsFS::DataBlockManager* dataBlockManager = nullptr;

    bool mounted = false;

    FilesystemError lastFilesystemError =
        FilesystemError::None;

    // Path tracking for 'cd ..'
    uint64_t directoryStack[16];
    uint32_t directoryStackDepth = 0;
    uint64_t currentDirectoryNode =
        TalonsFS::RootNode;

    char currentDirectoryPath[256] = "/";

    void setFilesystemError(
        FilesystemError error)
    {
        lastFilesystemError = error;
    }

    bool flushMetadata()
    {
        for (uint64_t i = 0;
             i < layout.nodeBitmapBlocks;
             ++i)
        {
            if (!device.writeBlock(
                    layout.nodeBitmapStart + i,
                    nodeBitmapBuffer + i * BLOCK_SIZE))
            {
                return false;
            }
        }

        for (uint64_t i = 0;
             i < layout.blockBitmapBlocks;
             ++i)
        {
            if (!device.writeBlock(
                    layout.blockBitmapStart + i,
                    blockBitmapBuffer + i * BLOCK_SIZE))
            {
                return false;
            }
        }

        for (uint64_t i = 0;
             i < layout.nodeTableBlocks;
             ++i)
        {
            if (!device.writeBlock(
                    layout.nodeTableStart + i,
                    nodeTableBuffer + i * BLOCK_SIZE))
            {
                return false;
            }
        }

        return true;
    }

    bool buildClaimedBlockMap()
    {
        __builtin_memset(
            recoveryClaimedBlocks,
            0,
            sizeof(recoveryClaimedBlocks)
        );

        for (uint64_t nodeIndex = 0;
             nodeIndex < nodeBitmap->size();
             ++nodeIndex)
        {
            if (!nodeBitmap->get(nodeIndex))
            {
                continue;
            }

            const TalonsFS::Node node =
                nodeTable->read(nodeIndex);

            if (node.dataBlocks == 0)
            {
                continue;
            }

            if (node.dataStart < layout.dataStart ||
                node.dataStart >= superblock.totalBlocks)
            {
                return false;
            }

            if (node.dataBlocks >
                superblock.totalBlocks - node.dataStart)
            {
                return false;
            }

            for (uint64_t i = 0;
                 i < node.dataBlocks;
                 ++i)
            {
                const uint64_t block =
                    node.dataStart + i;

                if (block >= superblock.totalBlocks)
                {
                    return false;
                }

                const uint64_t byteIndex =
                    block / 8;

                const uint8_t bitMask =
                    static_cast<uint8_t>(
                        1u << (block % 8)
                    );

                if (recoveryClaimedBlocks[byteIndex] &
                    bitMask)
                {
                    return false;
                }

                recoveryClaimedBlocks[byteIndex] |=
                    bitMask;
            }
        }

        return true;
    }

    bool recoverOrphanedBlocks()
    {
        if (!buildClaimedBlockMap())
        {
            return false;
        }

        bool changed = false;

        for (uint64_t block = layout.dataStart;
             block < superblock.totalBlocks;
             ++block)
        {
            const bool allocated =
                blockBitmap->get(block);

            const uint64_t byteIndex =
                block / 8;

            const uint8_t bitMask =
                static_cast<uint8_t>(
                    1u << (block % 8)
                );

            const bool claimed =
                (recoveryClaimedBlocks[byteIndex] &
                 bitMask) != 0;

            if (allocated && !claimed)
            {
                blockBitmap->clear(block);
                changed = true;
            }
        }

        if (!changed)
        {
            return true;
        }

        return flushMetadata();
    }

    bool appendDirectoryEntry(
        uint64_t directoryNode,
        const TalonsFS::DirectoryEntry& entry)
    {
        TalonsFS::Node node =
            nodeTable->read(directoryNode);

        if (node.type !=
            TalonsFS::NodeType::Directory)
        {
            return false;
        }

        constexpr uint64_t entrySize =
            sizeof(TalonsFS::DirectoryEntry);

        const uint64_t newSize =
            node.size + entrySize;

        const uint64_t requiredBlocks =
            (newSize + BLOCK_SIZE - 1) /
            BLOCK_SIZE;

        while (node.dataBlocks < requiredBlocks)
        {
            if (!nodeDataManager->allocateBlock(
                    directoryNode))
            {
                return false;
            }

            node =
                nodeTable->read(directoryNode);
        }

        const uint8_t* source =
            reinterpret_cast<const uint8_t*>(&entry);

        uint64_t remaining = entrySize;
        uint64_t currentOffset = node.size;

        while (remaining > 0)
        {
            const uint64_t blockIndex =
                currentOffset / BLOCK_SIZE;

            const uint64_t offsetInBlock =
                currentOffset % BLOCK_SIZE;

            const uint64_t blockNumber =
                node.dataStart + blockIndex;

            uint8_t blockBuf[BLOCK_SIZE];

            if (!dataBlockManager->read(
                    blockNumber,
                    blockBuf))
            {
                return false;
            }

            const uint64_t space =
                BLOCK_SIZE - offsetInBlock;

            const uint64_t bytesToCopy =
                remaining < space
                    ? remaining
                    : space;

            __builtin_memcpy(
                blockBuf + offsetInBlock,
                source,
                bytesToCopy
            );

            if (!dataBlockManager->write(
                    blockNumber,
                    blockBuf))
            {
                return false;
            }

            source += bytesToCopy;
            currentOffset += bytesToCopy;
            remaining -= bytesToCopy;
        }

        node.size = newSize;

        nodeTable->write(
            directoryNode,
            node
        );

        return true;
    }

    bool findDirectoryEntry(
        uint64_t directoryNode,
        const char* name,
        TalonsFS::DirectoryEntry& result)
    {
        if (name == nullptr ||
            name[0] == '\0')
        {
            return false;
        }

        TalonsFS::Node node =
            nodeTable->read(directoryNode);

        if (node.type !=
            TalonsFS::NodeType::Directory)
        {
            return false;
        }

        constexpr uint64_t entrySize =
            sizeof(TalonsFS::DirectoryEntry);

        if (node.size % entrySize != 0)
        {
            return false;
        }

        const uint64_t entryCount =
            node.size / entrySize;

        for (uint64_t entryIndex = 0;
             entryIndex < entryCount;
             ++entryIndex)
        {
            const uint64_t offset =
                entryIndex * entrySize;

            uint8_t entryBuf[entrySize];

            uint64_t remaining = entrySize;
            uint64_t currentOffset = offset;

            uint8_t* destination = entryBuf;

            while (remaining > 0)
            {
                const uint64_t blockIndex =
                    currentOffset / BLOCK_SIZE;

                const uint64_t offsetInBlock =
                    currentOffset % BLOCK_SIZE;

                if (blockIndex >= node.dataBlocks)
                {
                    return false;
                }

                const uint64_t blockNumber =
                    node.dataStart + blockIndex;

                uint8_t blockBuf[BLOCK_SIZE];

                if (!dataBlockManager->read(
                        blockNumber,
                        blockBuf))
                {
                    return false;
                }

                const uint64_t available =
                    BLOCK_SIZE - offsetInBlock;

                const uint64_t bytesToCopy =
                    remaining < available
                        ? remaining
                        : available;

                __builtin_memcpy(
                    destination,
                    blockBuf + offsetInBlock,
                    bytesToCopy
                );

                destination += bytesToCopy;
                currentOffset += bytesToCopy;
                remaining -= bytesToCopy;
            }

            TalonsFS::DirectoryEntry entry{};

            __builtin_memcpy(
                &entry,
                entryBuf,
                entrySize
            );

            bool nameMatches = true;

            for (uint32_t i = 0;
                 i < TalonsFS::MaxFilenameLength;
                 ++i)
            {
                if (entry.name[i] != name[i])
                {
                    nameMatches = false;
                    break;
                }

                if (entry.name[i] == '\0')
                {
                    break;
                }
            }

            if (nameMatches)
            {
                result = entry;
                return true;
            }
        }

        return false;
    }

    bool removeDirectoryEntry(
        uint64_t directoryNode,
        const char* name)
    {
        TalonsFS::Node node =
            nodeTable->read(directoryNode);

        if (node.type !=
            TalonsFS::NodeType::Directory)
        {
            return false;
        }

        constexpr uint64_t entrySize =
            sizeof(TalonsFS::DirectoryEntry);

        if (node.size % entrySize != 0)
        {
            return false;
        }

        const uint64_t entryCount =
            node.size / entrySize;

        for (uint64_t i = 0;
             i < entryCount;
             ++i)
        {
            const uint64_t offset =
                i * entrySize;

            uint8_t entryBuf[entrySize];

            uint64_t currentOffset = offset;
            uint8_t* dest = entryBuf;
            uint64_t rem = entrySize;

            while (rem > 0)
            {
                uint64_t bi =
                    currentOffset / BLOCK_SIZE;

                uint64_t oib =
                    currentOffset % BLOCK_SIZE;

                uint64_t bn =
                    node.dataStart + bi;

                uint8_t bb[BLOCK_SIZE];

                if (!dataBlockManager->read(
                        bn,
                        bb))
                {
                    return false;
                }

                uint64_t av =
                    BLOCK_SIZE - oib;

                uint64_t tc =
                    rem < av
                        ? rem
                        : av;

                __builtin_memcpy(
                    dest,
                    bb + oib,
                    tc
                );

                dest += tc;
                currentOffset += tc;
                rem -= tc;
            }

            TalonsFS::DirectoryEntry entry{};

            __builtin_memcpy(
                &entry,
                entryBuf,
                entrySize
            );

            bool match = true;

            for (uint32_t j = 0;
                 j < TalonsFS::MaxFilenameLength;
                 ++j)
            {
                if (entry.name[j] != name[j])
                {
                    match = false;
                    break;
                }

                if (entry.name[j] == '\0')
                {
                    break;
                }
            }

            if (match)
            {
                if (i < entryCount - 1)
                {
                    uint64_t lastOffset =
                        (entryCount - 1) * entrySize;

                    uint8_t lastBuf[entrySize];

                    currentOffset = lastOffset;
                    dest = lastBuf;
                    rem = entrySize;

                    while (rem > 0)
                    {
                        uint64_t bi =
                            currentOffset / BLOCK_SIZE;

                        uint64_t oib =
                            currentOffset % BLOCK_SIZE;

                        uint64_t bn =
                            node.dataStart + bi;

                        uint8_t bb[BLOCK_SIZE];

                        if (!dataBlockManager->read(
                                bn,
                                bb))
                        {
                            return false;
                        }

                        uint64_t av =
                            BLOCK_SIZE - oib;

                        uint64_t tc =
                            rem < av
                                ? rem
                                : av;

                        __builtin_memcpy(
                            dest,
                            bb + oib,
                            tc
                        );

                        dest += tc;
                        currentOffset += tc;
                        rem -= tc;
                    }

                    currentOffset = offset;

                    const uint8_t* src = lastBuf;
                    rem = entrySize;

                    while (rem > 0)
                    {
                        uint64_t bi =
                            currentOffset / BLOCK_SIZE;

                        uint64_t oib =
                            currentOffset % BLOCK_SIZE;

                        uint64_t bn =
                            node.dataStart + bi;

                        uint8_t bb[BLOCK_SIZE];

                        if (!dataBlockManager->read(
                                bn,
                                bb))
                        {
                            return false;
                        }

                        uint64_t sp =
                            BLOCK_SIZE - oib;

                        uint64_t tc =
                            rem < sp
                                ? rem
                                : sp;

                        __builtin_memcpy(
                            bb + oib,
                            src,
                            tc
                        );

                        if (!dataBlockManager->write(
                                bn,
                                bb))
                        {
                            return false;
                        }

                        src += tc;
                        currentOffset += tc;
                        rem -= tc;
                    }
                }

                node.size -= entrySize;

                nodeTable->write(
                    directoryNode,
                    node
                );

                return true;
            }
        }

        return false;
    }

    bool hasTalonsFileExtension(
    const char* name)
{
    if (name == nullptr)
    {
        return false;
    }

    uint32_t length = 0;

    while (name[length] != '\0')
    {
        ++length;
    }

    constexpr char extension[] = ".tal";
    constexpr uint32_t extensionLength = 4;

    if (length < extensionLength)
    {
        return false;
    }

    const uint32_t extensionStart =
        length - extensionLength;

    for (uint32_t i = 0;
         i < extensionLength;
         ++i)
    {
        if (name[extensionStart + i] !=
            extension[i])
        {
            return false;
        }
    }

    return true;
}

}

bool filesystem_init()
{
    mounted = false;
    lastFilesystemError =
        FilesystemError::None;

    directoryStackDepth = 0;
    currentDirectoryNode =
        TalonsFS::RootNode;

    currentDirectoryPath[0] = '/';
    currentDirectoryPath[1] = '\0';

    uint8_t sbBuf[BLOCK_SIZE] = {};

    if (!device.readBlock(
            1,
            sbBuf))
    {
        setFilesystemError(
            FilesystemError::DeviceError
        );

        return false;
    }

    __builtin_memcpy(
        &superblock,
        sbBuf,
        sizeof(TalonsFS::Superblock)
    );

    if (superblock.magic !=
        TalonsFS::TALONSFS_MAGIC)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (superblock.version !=
        TalonsFS::TALONSFS_VERSION)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (superblock.blockSize !=
        BLOCK_SIZE)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (superblock.totalBlocks !=
        device.getBlockCount())
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    layout =
        TalonsFS::calculateLayout(
            superblock.totalBlocks,
            superblock.blockSize
        );

    if (superblock.blockBitmapStart !=
        layout.blockBitmapStart)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.blockBitmapBlocks !=
        layout.blockBitmapBlocks)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.nodeBitmapStart !=
        layout.nodeBitmapStart)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.nodeBitmapBlocks !=
        layout.nodeBitmapBlocks)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.nodeTableStart !=
        layout.nodeTableStart)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.nodeTableBlocks !=
        layout.nodeTableBlocks)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.nodeCount !=
        layout.nodeCount)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.dataStart !=
        layout.dataStart)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (superblock.rootNode !=
        TalonsFS::RootNode)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    /*
     * Every field checked above is cross-validated against layout
     * independently recomputed from totalBlocks/blockSize, which
     * catches structural corruption. It does NOT cover fields with no
     * independent source of truth to check against -- freeBlocks,
     * freeNodes, and filesystemID -- nor does it catch the case where
     * several fields are corrupted together in a way that happens to
     * stay self-consistent. The checksum, written by
     * filesystem_format() over the whole superblock, is the only
     * check that covers those bytes.
     */
    if (TalonsFS::calculateSuperblockChecksum(superblock) !=
        superblock.checksum)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (layout.nodeTableBlocks >
        NODE_TABLE_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (layout.blockBitmapBlocks >
        BLOCK_BITMAP_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (layout.nodeBitmapBlocks >
        NODE_BITMAP_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    for (uint64_t i = 0;
         i < layout.nodeBitmapBlocks;
         ++i)
    {
        if (!device.readBlock(
                layout.nodeBitmapStart + i,
                nodeBitmapBuffer + i * BLOCK_SIZE))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }
    }

    for (uint64_t i = 0;
         i < layout.blockBitmapBlocks;
         ++i)
    {
        if (!device.readBlock(
                layout.blockBitmapStart + i,
                blockBitmapBuffer + i * BLOCK_SIZE))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }
    }

    for (uint64_t i = 0;
         i < layout.nodeTableBlocks;
         ++i)
    {
        if (!device.readBlock(
                layout.nodeTableStart + i,
                nodeTableBuffer + i * BLOCK_SIZE))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }
    }

    blockBitmap =
        new (blockBitmapStorage)
        TalonsFS::Bitmap(
            blockBitmapBuffer,
            superblock.totalBlocks
        );

    nodeBitmap =
        new (nodeBitmapStorage)
        TalonsFS::Bitmap(
            nodeBitmapBuffer,
            layout.nodeCount
        );

    nodeTable =
        new (nodeTableStorage)
        TalonsFS::NodeTable(
            nodeTableBuffer,
            layout.nodeCount
        );

    blockAllocator =
        new (blockAllocatorStorage)
        TalonsFS::BlockAllocator(
            *blockBitmap,
            superblock.totalBlocks,
            layout.dataStart
        );

    nodeAllocator =
        new (nodeAllocatorStorage)
        TalonsFS::NodeAllocator(
            *nodeBitmap,
            *nodeTable
        );

    nodeManager =
        new (nodeManagerStorage)
        TalonsFS::NodeManager(
            *nodeAllocator,
            *nodeTable
        );

    nodeDataManager =
        new (nodeDataManagerStorage)
        TalonsFS::NodeDataManager(
            *blockAllocator,
            *nodeTable
        );

    dataBlockManager =
        new (dataBlockManagerStorage)
        TalonsFS::DataBlockManager(
            device
        );

    if (!nodeBitmap->get(
            TalonsFS::RootNode))
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    const TalonsFS::Node root =
        nodeTable->read(
            TalonsFS::RootNode
        );

    if (root.type !=
        TalonsFS::NodeType::Directory)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (!recoverOrphanedBlocks())
    {
        setFilesystemError(
            FilesystemError::RecoveryError
        );

        return false;
    }

    mounted = true;

if (!pipe_manager_init())
{
    mounted = false;

    setFilesystemError(
        FilesystemError::MetadataError
    );

    return false;
}

if (!file_descriptors_init())

    {
        mounted = false;

        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_unmount()
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::AlreadyUnmounted
        );

        return false;
    }

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    /*
     * flushMetadata() only writes into the BlockCache, which is
     * write-back: without this the metadata (and any file data
     * still held in dirty entries) never reaches the platter.
     */
    if (!device.flush())
    {
        setFilesystemError(
            FilesystemError::DeviceError
        );

        return false;
    }

    mounted = false;

    directoryStackDepth = 0;

    currentDirectoryNode =
        TalonsFS::RootNode;

    currentDirectoryPath[0] = '/';
    currentDirectoryPath[1] = '\0';

    return true;
}

bool filesystem_is_mounted()
{
    return mounted;
}

FilesystemError filesystem_get_last_error()
{
    return lastFilesystemError;
}

const char* filesystem_error_string(
    FilesystemError error)
{
    switch (error)
    {
        case FilesystemError::None:              return "no error";
        case FilesystemError::NotMounted:        return "filesystem not mounted";
        case FilesystemError::InvalidArgument:   return "invalid argument";
        case FilesystemError::NotFound:          return "not found";
        case FilesystemError::AlreadyExists:     return "already exists";
        case FilesystemError::NotAFile:          return "not a file";
        case FilesystemError::NotADirectory:     return "not a directory";
        case FilesystemError::DirectoryNotEmpty: return "directory not empty";
        case FilesystemError::NoSpace:           return "no space left";
        case FilesystemError::InvalidFilesystem: return "invalid filesystem";
        case FilesystemError::CorruptFilesystem: return "corrupt filesystem";
        case FilesystemError::DeviceError:       return "device error";
        case FilesystemError::MetadataError:     return "metadata error";
        case FilesystemError::RecoveryError:     return "recovery error";
        case FilesystemError::AlreadyUnmounted:  return "already unmounted";
        case FilesystemError::Unknown:           return "unknown error";
    }

    return "unknown error";
}

bool filesystem_format()
{
    mounted = false;

    lastFilesystemError =
        FilesystemError::None;

    const uint32_t blockSize =
        device.getBlockSize();

    const uint64_t totalBlocks =
        device.getBlockCount();

    if (blockSize != BLOCK_SIZE ||
        totalBlocks < 2)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    layout =
        TalonsFS::calculateLayout(
            totalBlocks,
            blockSize
        );

    if (layout.nodeCount == 0 ||
        layout.dataStart >= totalBlocks)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (layout.nodeTableBlocks >
        NODE_TABLE_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (layout.blockBitmapBlocks >
        BLOCK_BITMAP_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    if (layout.nodeBitmapBlocks >
        NODE_BITMAP_CAPACITY_BLOCKS)
    {
        setFilesystemError(
            FilesystemError::InvalidFilesystem
        );

        return false;
    }

    /*
     * Start with completely clean metadata buffers.
     */
    __builtin_memset(
        nodeTableBuffer,
        0,
        sizeof(nodeTableBuffer)
    );

    __builtin_memset(
        blockBitmapBuffer,
        0,
        sizeof(blockBitmapBuffer)
    );

    __builtin_memset(
        nodeBitmapBuffer,
        0,
        sizeof(nodeBitmapBuffer)
    );

    /*
     * Filesystem metadata occupies every block
     * before dataStart.
     */
    blockBitmap =
        new (blockBitmapStorage)
        TalonsFS::Bitmap(
            blockBitmapBuffer,
            totalBlocks
        );

    for (uint64_t block = 0;
         block < layout.dataStart;
         ++block)
    {
        blockBitmap->set(block);
    }

    nodeBitmap =
        new (nodeBitmapStorage)
        TalonsFS::Bitmap(
            nodeBitmapBuffer,
            layout.nodeCount
        );

    nodeTable =
        new (nodeTableStorage)
        TalonsFS::NodeTable(
            nodeTableBuffer,
            layout.nodeCount
        );

    /*
     * Create the allocators and node manager
     * using the freshly formatted metadata.
     */
    nodeAllocator =
        new (nodeAllocatorStorage)
        TalonsFS::NodeAllocator(
            *nodeBitmap,
            *nodeTable
        );

    nodeManager =
        new (nodeManagerStorage)
        TalonsFS::NodeManager(
            *nodeAllocator,
            *nodeTable
        );

    /*
     * Root must be node 0.
     *
     * NodeManager::create() also gives it the
     * current permissions and timestamps.
     */
    const int64_t rootNode =
        nodeManager->create(
            TalonsFS::NodeType::Directory
        );

    if (rootNode !=
        static_cast<int64_t>(
            TalonsFS::RootNode))
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    /*
     * Build the new superblock.
     */
    superblock = {};

    superblock.magic =
        TalonsFS::TALONSFS_MAGIC;

    superblock.version =
        TalonsFS::TALONSFS_VERSION;

    superblock.blockSize =
        blockSize;

    superblock.totalBlocks =
        totalBlocks;

    superblock.blockBitmapStart =
        layout.blockBitmapStart;

    superblock.blockBitmapBlocks =
        layout.blockBitmapBlocks;

    superblock.nodeBitmapStart =
        layout.nodeBitmapStart;

    superblock.nodeBitmapBlocks =
        layout.nodeBitmapBlocks;

    superblock.nodeTableStart =
        layout.nodeTableStart;

    superblock.nodeTableBlocks =
        layout.nodeTableBlocks;

    superblock.nodeCount =
        layout.nodeCount;

    superblock.dataStart =
        layout.dataStart;

    superblock.rootNode =
        TalonsFS::RootNode;

    superblock.freeBlocks =
        totalBlocks - layout.dataStart;

    superblock.freeNodes =
        layout.nodeCount - 1;

    superblock.filesystemID =
        TalonsFS::TALONSFS_MAGIC;

    superblock.checksum =
        TalonsFS::calculateSuperblockChecksum(
            superblock
        );

    /*
     * Write the freshly-created metadata.
     */
    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    /*
     * ORDERING BARRIER.
     *
     * flushMetadata() only populates the write-back cache. Without
     * this the cache's round-robin eviction is free to push the
     * superblock (written below) to the platter BEFORE the metadata,
     * because eviction order follows entry index, not write order.
     *
     * Draining the cache here restores the guarantee at the physical
     * device without disabling caching anywhere else.
     */
    if (!device.flush())
    {
        setFilesystemError(
            FilesystemError::DeviceError
        );

        return false;
    }

    /*
     * Write the superblock last so that the filesystem
     * is not advertised as valid until its metadata exists.
     */
    uint8_t sbBuffer[BLOCK_SIZE] = {};

    __builtin_memcpy(
        sbBuffer,
        &superblock,
        sizeof(TalonsFS::Superblock)
    );

    if (!device.writeBlock(
            layout.superblockBlock,
            sbBuffer))
    {
        setFilesystemError(
            FilesystemError::DeviceError
        );

        return false;
    }

    /*
     * Commit the superblock too, so that a format which reports
     * success has actually reached the disk.
     */
    if (!device.flush())
    {
        setFilesystemError(
            FilesystemError::DeviceError
        );

        return false;
    }

    /*
     * Reload everything through the normal mount path.
     * This makes formatting use exactly the same runtime
     * initialization path as an ordinary boot.
     */
    if (!filesystem_init())
    {
        return false;
    }

    return mounted;
}

unsigned int filesystem_list_directory(
    kfs::ListEntry* out)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return 0;
    }

    if (out == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return 0;
    }

    const TalonsFS::Node dir =
        nodeTable->read(
            currentDirectoryNode
        );

    if (dir.type !=
        TalonsFS::NodeType::Directory)
    {
        setFilesystemError(
            FilesystemError::NotADirectory
        );

        return 0;
    }

    constexpr uint64_t entrySize =
        sizeof(TalonsFS::DirectoryEntry);

    if (dir.size % entrySize != 0)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return 0;
    }

    const uint64_t entryCount =
        dir.size / entrySize;

    unsigned int written = 0;

    for (uint64_t i = 0;
         i < entryCount &&
         written < kfs::MAX_LIST_ENTRIES;
         ++i)
    {
        const uint64_t offset =
            i * entrySize;

        uint8_t entryBuf[entrySize];

        uint64_t remaining = entrySize;
        uint64_t currentOffset = offset;

        uint8_t* destination = entryBuf;

        bool ok = true;

        while (remaining > 0)
        {
            const uint64_t blockIndex =
                currentOffset / BLOCK_SIZE;

            const uint64_t offsetInBlock =
                currentOffset % BLOCK_SIZE;

            if (blockIndex >= dir.dataBlocks)
            {
                ok = false;
                break;
            }

            const uint64_t blockNumber =
                dir.dataStart + blockIndex;

            uint8_t blockBuf[BLOCK_SIZE];

            if (!dataBlockManager->read(
                    blockNumber,
                    blockBuf))
            {
                ok = false;
                break;
            }

            const uint64_t available =
                BLOCK_SIZE - offsetInBlock;

            const uint64_t bytesToCopy =
                remaining < available
                    ? remaining
                    : available;

            __builtin_memcpy(
                destination,
                blockBuf + offsetInBlock,
                bytesToCopy
            );

            destination += bytesToCopy;
            currentOffset += bytesToCopy;
            remaining -= bytesToCopy;
        }

        if (!ok)
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            break;
        }

        TalonsFS::DirectoryEntry entry{};

        __builtin_memcpy(
            &entry,
            entryBuf,
            entrySize
        );

        if (entry.name[0] != '\0')
        {
            __builtin_memcpy(
                out[written].name,
                entry.name,
                TalonsFS::MaxFilenameLength
            );

            out[written].name[
                TalonsFS::MaxFilenameLength - 1
            ] = '\0';

            out[written].isDirectory =
                (entry.type ==
                 static_cast<uint8_t>(
                     TalonsFS::NodeType::Directory));

            ++written;
        }
    }

    return written;
}

bool filesystem_create_directory(
    const char* name)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
        name[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    uint32_t len = 0;

    while (name[len] != '\0')
    {
        ++len;

        if (len >=
            TalonsFS::MaxFilenameLength)
        {
            setFilesystemError(
                FilesystemError::InvalidArgument
            );

            return false;
        }
    }

    TalonsFS::DirectoryEntry existing{};

    if (findDirectoryEntry(
            currentDirectoryNode,
            name,
            existing))
    {
        setFilesystemError(
            FilesystemError::AlreadyExists
        );

        return false;
    }

    const int64_t newNode =
        nodeManager->create(
            TalonsFS::NodeType::Directory
        );

    if (newNode < 0)
    {
        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    entry.node =
        static_cast<uint64_t>(newNode);

    entry.type =
        static_cast<uint8_t>(
            TalonsFS::NodeType::Directory
        );

    for (uint32_t i = 0;
         i < TalonsFS::MaxFilenameLength;
         ++i)
    {
        entry.name[i] = 0;
    }

    __builtin_memcpy(
        entry.name,
        name,
        len
    );

    if (!appendDirectoryEntry(
            currentDirectoryNode,
            entry))
    {
        nodeManager->remove(
            static_cast<uint64_t>(newNode)
        );

        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_create_file(
    const char* name)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
    name[0] == '\0')
{
    setFilesystemError(
        FilesystemError::InvalidArgument
    );

    return false;
}

if (!hasTalonsFileExtension(name))
{
    setFilesystemError(
        FilesystemError::InvalidArgument
    );

    return false;
}

uint32_t len = 0;

while (name[len] != '\0')
{
    ++len;

    if (len >=
        TalonsFS::MaxFilenameLength)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }
}

    TalonsFS::DirectoryEntry existing{};

    if (findDirectoryEntry(
            currentDirectoryNode,
            name,
            existing))
    {
        setFilesystemError(
            FilesystemError::AlreadyExists
        );

        return false;
    }

    const int64_t newNode =
        nodeManager->create(
            TalonsFS::NodeType::File
        );

    if (newNode < 0)
    {
        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    entry.node =
        static_cast<uint64_t>(newNode);

    entry.type =
        static_cast<uint8_t>(
            TalonsFS::NodeType::File
        );

    for (uint32_t i = 0;
         i < TalonsFS::MaxFilenameLength;
         ++i)
    {
        entry.name[i] = 0;
    }

    __builtin_memcpy(
        entry.name,
        name,
        len
    );

    if (!appendDirectoryEntry(
            currentDirectoryNode,
            entry))
    {
        nodeManager->remove(
            static_cast<uint64_t>(newNode)
        );

        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

int filesystem_open(
    const char* name,
    uint32_t flags)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return -1;
    }

    if (name == nullptr ||
        name[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    if ((flags &
         (FILESYSTEM_OPEN_READ |
          FILESYSTEM_OPEN_WRITE)) == 0)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    if (flags &
        ~(FILESYSTEM_OPEN_READ |
          FILESYSTEM_OPEN_WRITE))
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    TalonsFS::DirectoryEntry entry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            name,
            entry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return -1;
    }

    if (entry.type !=
        static_cast<uint8_t>(
            TalonsFS::NodeType::File))
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return -1;
    }

    const int fd =
        file_descriptor_allocate(
            entry.node,
            flags
        );

    if (fd < 0)
    {
        setFilesystemError(
            FilesystemError::NoSpace
        );

        return -1;
    }

    return fd;
}

bool filesystem_create_pipe(
    int& readFd,
    int& writeFd
)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    const int pipeId =
        pipe_manager_create();

    if (pipeId < 0)
    {
        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    readFd =
        file_descriptor_allocate_pipe(
            static_cast<uint32_t>(pipeId),
            FILESYSTEM_OPEN_READ
        );

    if (readFd < 0)
    {
        pipe_manager_destroy(pipeId);

        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    writeFd =
        file_descriptor_allocate_pipe(
            static_cast<uint32_t>(pipeId),
            FILESYSTEM_OPEN_WRITE
        );

    if (writeFd < 0)
    {
        file_descriptor_close(readFd);
        pipe_manager_destroy(pipeId);

        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    return true;
}


bool filesystem_close(int fd)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr ||
        fd < FIRST_USER_FILE_DESCRIPTOR)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    // Save pipe information before releasing the descriptor.
    const bool isPipe =
        descriptor->type == FileDescriptorType::Pipe;

    const uint32_t pipeId = descriptor->pipeId;
    const uint32_t flags = descriptor->flags;

    Pipe* pipe = nullptr;

    if (isPipe)
    {
        pipe = pipe_manager_get(
            static_cast<int>(pipeId)
        );

        if (pipe == nullptr)
        {
            setFilesystemError(
                FilesystemError::InvalidArgument
            );

            return false;
        }
    }

    if (!file_descriptor_close(fd))
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    if (isPipe)
    {
        if (flags & FILESYSTEM_OPEN_READ)
            pipe_close_read(*pipe);

        if (flags & FILESYSTEM_OPEN_WRITE)
            pipe_close_write(*pipe);

        if (!pipe->readOpen && !pipe->writeOpen)
        {
            if (!pipe_manager_destroy(
                    static_cast<int>(pipeId)))
            {
                setFilesystemError(
                    FilesystemError::MetadataError
                );

                return false;
            }
        }
    }

    return true;
}

int filesystem_read(
    int fd,
    uint8_t* out,
    uint32_t outCapacity)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return -1;
    }

    if (out == nullptr ||
        outCapacity == 0)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    if ((descriptor->flags &
         FILESYSTEM_OPEN_READ) == 0)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    if ((descriptor->flags &
     FILESYSTEM_OPEN_READ) == 0)
{
    setFilesystemError(
        FilesystemError::InvalidArgument
    );

    return -1;
}

if (descriptor->type == FileDescriptorType::Pipe)
{
    Pipe* pipe =
        pipe_manager_get(
            static_cast<int>(
                descriptor->pipeId
            )
        );

    if (pipe == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return -1;
    }

    return pipe_read(
        *pipe,
        out,
        outCapacity
    ) ? static_cast<int>(outCapacity) : -1;
}

    const TalonsFS::Node node =
        nodeTable->read(
            descriptor->node
        );

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return -1;
    }

    if (descriptor->offset >= node.size)
    {
        return 0;
    }

    if (node.dataBlocks == 0)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return -1;
    }

    const uint64_t remaining =
        node.size - descriptor->offset;

    uint32_t bytesToRead =
        outCapacity;

    if (remaining <
        static_cast<uint64_t>(
            bytesToRead))
    {
        bytesToRead =
            static_cast<uint32_t>(
                remaining
            );
    }

    uint8_t* destination = out;

    uint64_t currentOffset =
        descriptor->offset;

    uint32_t remainingToRead =
        bytesToRead;

    while (remainingToRead > 0)
    {
        const uint64_t blockIndex =
            currentOffset / BLOCK_SIZE;

        const uint64_t offsetInBlock =
            currentOffset % BLOCK_SIZE;

        if (blockIndex >= node.dataBlocks)
        {
            setFilesystemError(
                FilesystemError::CorruptFilesystem
            );

            return -1;
        }

        const uint64_t blockNumber =
            node.dataStart + blockIndex;

        uint8_t blockBuf[BLOCK_SIZE];

        if (!dataBlockManager->read(
                blockNumber,
                blockBuf))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return -1;
        }

        const uint64_t available =
            BLOCK_SIZE - offsetInBlock;

        uint32_t bytesToCopy =
            remainingToRead;

        if (available <
            static_cast<uint64_t>(
                bytesToCopy))
        {
            bytesToCopy =
                static_cast<uint32_t>(
                    available
                );
        }

        __builtin_memcpy(
            destination,
            blockBuf + offsetInBlock,
            bytesToCopy
        );

        destination += bytesToCopy;

        currentOffset +=
            bytesToCopy;

        remainingToRead -=
            bytesToCopy;
    }

    descriptor->offset =
        currentOffset;

    return static_cast<int>(
        bytesToRead
    );
}

bool filesystem_read_file(
    const char* name,
    uint8_t* out,
    uint32_t outCapacity,
    uint64_t* outSize)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
        name[0] == '\0' ||
        out == nullptr ||
        outSize == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            name,
            entry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    const TalonsFS::Node node =
        nodeTable->read(entry.node);

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    const uint64_t size = node.size;

    if (size == 0)
    {
        *outSize = 0;
        return true;
    }

    if (node.dataBlocks == 0)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    if (size > outCapacity)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    uint8_t* destination = out;

    uint64_t remaining = size;

    for (uint64_t i = 0;
         i < node.dataBlocks &&
         remaining > 0;
         ++i)
    {
        uint8_t blockBuf[BLOCK_SIZE];

        if (!dataBlockManager->read(
                node.dataStart + i,
                blockBuf))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }

        const uint64_t bytesToCopy =
            remaining < BLOCK_SIZE
                ? remaining
                : BLOCK_SIZE;

        __builtin_memcpy(
            destination,
            blockBuf,
            bytesToCopy
        );

        destination += bytesToCopy;
        remaining -= bytesToCopy;
    }

    if (remaining != 0)
    {
        setFilesystemError(
            FilesystemError::CorruptFilesystem
        );

        return false;
    }

    *outSize = size;

    return true;
}

bool filesystem_write(
    int fd,
    const uint8_t* data,
    uint32_t size)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (data == nullptr ||
        size == 0)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    if ((descriptor->flags &
         FILESYSTEM_OPEN_WRITE) == 0)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

        if (descriptor->type == FileDescriptorType::Pipe)
    {
        Pipe* pipe =
            pipe_manager_get(
                static_cast<int>(
                    descriptor->pipeId
                )
            );

        if (pipe == nullptr)
        {
            setFilesystemError(
                FilesystemError::InvalidArgument
            );

            return false;
        }

        if (!pipe_write(
                *pipe,
                data,
                size))
        {
            setFilesystemError(
                FilesystemError::InvalidArgument
            );

            return false;
        }

        return true;
    }

    TalonsFS::Node node =
        nodeTable->read(
            descriptor->node
        );

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    const uint64_t writeStart =
        descriptor->offset;

    const uint64_t writeEnd =
        writeStart + size;

    if (writeEnd < writeStart)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    const uint64_t requiredBlocks =
        (writeEnd + BLOCK_SIZE - 1) /
        BLOCK_SIZE;

    while (node.dataBlocks < requiredBlocks)
    {
        if (!nodeDataManager->allocateBlock(
                descriptor->node))
        {
            setFilesystemError(
                FilesystemError::NoSpace
            );

            return false;
        }

        node =
            nodeTable->read(
                descriptor->node
            );
    }

    uint64_t currentOffset =
        writeStart;

    const uint8_t* source =
        data;

    uint32_t remaining =
        size;

    while (remaining > 0)
    {
        const uint64_t blockIndex =
            currentOffset / BLOCK_SIZE;

        const uint64_t offsetInBlock =
            currentOffset % BLOCK_SIZE;

        if (blockIndex >= node.dataBlocks)
        {
            setFilesystemError(
                FilesystemError::CorruptFilesystem
            );

            return false;
        }

        const uint64_t blockNumber =
            node.dataStart + blockIndex;

        uint8_t blockBuf[BLOCK_SIZE] = {};

        const uint64_t bytesToCopy =
            (BLOCK_SIZE - offsetInBlock <
             static_cast<uint64_t>(remaining))
                ? BLOCK_SIZE - offsetInBlock
                : remaining;

        if (offsetInBlock != 0 ||
            bytesToCopy < BLOCK_SIZE)
        {
            if (!dataBlockManager->read(
                    blockNumber,
                    blockBuf))
            {
                setFilesystemError(
                    FilesystemError::DeviceError
                );

                return false;
            }
        }

        __builtin_memcpy(
            blockBuf + offsetInBlock,
            source,
            bytesToCopy
        );

        if (!dataBlockManager->write(
                blockNumber,
                blockBuf))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }

        source += bytesToCopy;

        currentOffset +=
            bytesToCopy;

        remaining -=
            static_cast<uint32_t>(
                bytesToCopy
            );
    }

    if (writeEnd > node.size)
    {
        node.size = writeEnd;

        nodeTable->write(
            descriptor->node,
            node
        );
    }

    descriptor->offset =
        writeEnd;

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_seek(
    int fd,
    uint64_t offset)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    const TalonsFS::Node node =
        nodeTable->read(
            descriptor->node
        );

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    if (offset > node.size)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    descriptor->offset =
        offset;

    return true;
}

bool filesystem_get_file_size(
    int fd,
    uint64_t* size)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (size == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    const TalonsFS::Node node =
        nodeTable->read(
            descriptor->node
        );

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    *size = node.size;

    return true;
}

bool filesystem_write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
        data == nullptr)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            name,
            entry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    TalonsFS::Node node =
        nodeTable->read(entry.node);

    if (node.type !=
        TalonsFS::NodeType::File)
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    /*
     * Overwrite behavior:
     * free old blocks and start fresh.
     */
    if (node.dataBlocks > 0)
    {
        if (!nodeDataManager->freeBlocks(
                entry.node))
        {
            setFilesystemError(
                FilesystemError::MetadataError
            );

            return false;
        }

        node =
            nodeTable->read(entry.node);
    }

    uint64_t requiredBlocks =
        (size + BLOCK_SIZE - 1) /
        BLOCK_SIZE;

    for (uint64_t i = 0;
         i < requiredBlocks;
         ++i)
    {
        if (!nodeDataManager->allocateBlock(
                entry.node))
        {
            setFilesystemError(
                FilesystemError::NoSpace
            );

            return false;
        }
    }

    node =
        nodeTable->read(entry.node);

    uint32_t written = 0;

    while (written < size)
    {
        uint64_t bi =
            written / BLOCK_SIZE;

        uint64_t oib =
            written % BLOCK_SIZE;

        uint64_t bn =
            node.dataStart + bi;

        uint8_t bb[BLOCK_SIZE] = {0};

        uint32_t tc =
            BLOCK_SIZE - oib;

        if (tc > size - written)
        {
            tc = size - written;
        }

        if (oib != 0 ||
            tc < BLOCK_SIZE)
        {
            if (!dataBlockManager->read(
                    bn,
                    bb))
            {
                setFilesystemError(
                    FilesystemError::DeviceError
                );

                return false;
            }
        }

        __builtin_memcpy(
            bb + oib,
            data + written,
            tc
        );

        if (!dataBlockManager->write(
                bn,
                bb))
        {
            setFilesystemError(
                FilesystemError::DeviceError
            );

            return false;
        }

        written += tc;
    }

    node.size = size;

    nodeTable->write(
        entry.node,
        node
    );

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_remove(
    const char* name)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
        name[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            name,
            entry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    if (entry.type ==
        static_cast<uint8_t>(
            TalonsFS::NodeType::Directory))
    {
        TalonsFS::Node dir =
            nodeTable->read(entry.node);

        if (dir.size > 0)
        {
            /*
             * The target IS a directory -- it just is not empty.
             * Reporting NotADirectory here told the caller the exact
             * opposite of the truth.
             */
            setFilesystemError(
                FilesystemError::DirectoryNotEmpty
            );

            return false;
        }
    }

    if (!nodeDataManager->freeBlocks(
            entry.node))
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    if (!nodeManager->remove(
            entry.node))
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    if (!removeDirectoryEntry(
            currentDirectoryNode,
            name))
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_copy(
    const char* src,
    const char* dst)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (src == nullptr ||
        dst == nullptr ||
        src[0] == '\0' ||
        dst[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry srcEntry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            src,
            srcEntry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    if (srcEntry.type !=
        static_cast<uint8_t>(
            TalonsFS::NodeType::File))
    {
        setFilesystemError(
            FilesystemError::NotAFile
        );

        return false;
    }

    TalonsFS::DirectoryEntry dstEntry{};

    if (findDirectoryEntry(
            currentDirectoryNode,
            dst,
            dstEntry))
    {
        setFilesystemError(
            FilesystemError::AlreadyExists
        );

        return false;
    }

    if (!filesystem_create_file(dst))
    {
        return false;
    }

    static uint8_t buffer[
        kfs::MAX_FILE_READ_BYTES
    ];

    uint64_t size = 0;

    if (!filesystem_read_file(
            src,
            buffer,
            sizeof(buffer),
            &size))
    {
        /*
         * Capture the real cause BEFORE rolling back: every public
         * filesystem_* entry point resets lastFilesystemError to None
         * on entry, so filesystem_remove() would otherwise erase the
         * error we are about to report and the caller would see None.
         */
        const FilesystemError cause = lastFilesystemError;

        filesystem_remove(dst);

        setFilesystemError(cause);

        return false;
    }

    if (!filesystem_write_file(
            dst,
            buffer,
            static_cast<uint32_t>(size)))
    {
        const FilesystemError cause = lastFilesystemError;

        filesystem_remove(dst);

        setFilesystemError(cause);

        return false;
    }

    return true;
}

bool filesystem_move(
    const char* src,
    const char* dst)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (src == nullptr ||
        dst == nullptr ||
        src[0] == '\0' ||
        dst[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry srcEntry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            src,
            srcEntry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    TalonsFS::DirectoryEntry dstExisting{};

    if (findDirectoryEntry(
            currentDirectoryNode,
            dst,
            dstExisting))
    {
        setFilesystemError(
            FilesystemError::AlreadyExists
        );

        return false;
    }

    TalonsFS::DirectoryEntry newEntry =
        srcEntry;

    uint32_t len = 0;

    while (dst[len] != '\0' &&
           len <
               TalonsFS::MaxFilenameLength - 1)
    {
        newEntry.name[len] = dst[len];
        len++;
    }

    if (dst[len] != '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    newEntry.name[len] = '\0';

    for (uint32_t i = len + 1;
         i < TalonsFS::MaxFilenameLength;
         ++i)
    {
        newEntry.name[i] = 0;
    }

    if (!appendDirectoryEntry(
            currentDirectoryNode,
            newEntry))
    {
        setFilesystemError(
            FilesystemError::NoSpace
        );

        return false;
    }

    if (!removeDirectoryEntry(
            currentDirectoryNode,
            src))
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    if (!flushMetadata())
    {
        setFilesystemError(
            FilesystemError::MetadataError
        );

        return false;
    }

    return true;
}

bool filesystem_change_directory(
    const char* name)
{
    lastFilesystemError =
        FilesystemError::None;

    if (!mounted)
    {
        setFilesystemError(
            FilesystemError::NotMounted
        );

        return false;
    }

    if (name == nullptr ||
        name[0] == '\0')
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    if (name[0] == '.' &&
        name[1] == '.' &&
        name[2] == '\0')
    {
        if (directoryStackDepth > 0)
        {
            directoryStackDepth--;

            currentDirectoryNode =
                directoryStack[
                    directoryStackDepth
                ];

            unsigned int length = 0;

            while (
                currentDirectoryPath[length] != '\0' &&
                length <
                    sizeof(currentDirectoryPath) - 1)
            {
                length++;
            }

            if (length > 1)
            {
                length--;

                while (
                    length > 0 &&
                    currentDirectoryPath[length - 1] != '/')
                {
                    length--;
                }

                if (length > 1)
                {
                    length--;
                }

                currentDirectoryPath[length] =
                    '\0';
            }
            else
            {
                currentDirectoryPath[0] = '/';
                currentDirectoryPath[1] = '\0';
            }

            return true;
        }

        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    TalonsFS::DirectoryEntry entry{};

    if (!findDirectoryEntry(
            currentDirectoryNode,
            name,
            entry))
    {
        setFilesystemError(
            FilesystemError::NotFound
        );

        return false;
    }

    if (entry.type !=
        static_cast<uint8_t>(
            TalonsFS::NodeType::Directory))
    {
        setFilesystemError(
            FilesystemError::NotADirectory
        );

        return false;
    }

    if (directoryStackDepth >= 16)
    {
        setFilesystemError(
            FilesystemError::InvalidArgument
        );

        return false;
    }

    directoryStack[
        directoryStackDepth++
    ] = currentDirectoryNode;

    currentDirectoryNode =
        entry.node;

    unsigned int length = 0;

    while (
        currentDirectoryPath[length] != '\0' &&
        length <
            sizeof(currentDirectoryPath) - 1)
    {
        length++;
    }

    currentDirectoryPath[length++] = '/';

    unsigned int nameLength = 0;

    while (
        name[nameLength] != '\0' &&
        length <
            sizeof(currentDirectoryPath) - 1)
    {
        currentDirectoryPath[length++] =
            name[nameLength++];
    }

    currentDirectoryPath[length] =
        '\0';

    return true;
}

const char* filesystem_current_directory_path()
{
    return currentDirectoryPath;
}
