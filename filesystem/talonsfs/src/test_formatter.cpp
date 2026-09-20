#include <iostream>
#include <cstdint>
#include <vector>
#include <cstring>

#include "../include/talonsfs/memory_block_device.h"
#include "../include/talonsfs/formatter.h"
#include "../include/talonsfs/layout.h"
#include "../include/talonsfs/node.h"

int main()
{
    constexpr uint64_t totalBlocks = 4096;
    constexpr uint32_t blockSize = 4096;

    TalonsFS::MemoryBlockDevice device(
        totalBlocks,
        blockSize
    );

    TalonsFS::Superblock superblock{};

    bool formatted =
        TalonsFS::format(
            device,
            superblock
        );

    const TalonsFS::FilesystemLayout layout =
        TalonsFS::calculateLayout(
            totalBlocks,
            blockSize
        );

    std::cout << "=== TalonsFS Formatter Test ===\n\n";

    std::cout << "Formatted: "
              << formatted
              << '\n';

    std::cout << "\n=== Layout ===\n";

    std::cout << "Superblock block: "
              << layout.superblockBlock
              << '\n';

    std::cout << "Block bitmap start: "
              << layout.blockBitmapStart
              << '\n';

    std::cout << "Block bitmap blocks: "
              << layout.blockBitmapBlocks
              << '\n';

    std::cout << "Node bitmap start: "
              << layout.nodeBitmapStart
              << '\n';

    std::cout << "Node bitmap blocks: "
              << layout.nodeBitmapBlocks
              << '\n';

    std::cout << "Node count: "
              << layout.nodeCount
              << '\n';

    std::cout << "Node table start: "
              << layout.nodeTableStart
              << '\n';

    std::cout << "Node table blocks: "
              << layout.nodeTableBlocks
              << '\n';

    std::cout << "Data start: "
              << layout.dataStart
              << '\n';

    std::cout << "\n=== Superblock ===\n";

    std::cout << "Magic: 0x"
              << std::hex
              << superblock.magic
              << std::dec
              << '\n';

    std::cout << "Version: "
              << superblock.version
              << '\n';

    std::cout << "Block size: "
              << superblock.blockSize
              << '\n';

    std::cout << "Total blocks: "
              << superblock.totalBlocks
              << '\n';

    std::cout << "Root node: "
              << superblock.rootNode
              << '\n';

    std::cout << "Node table start: "
              << superblock.nodeTableStart
              << '\n';

    std::cout << "Node table blocks: "
              << superblock.nodeTableBlocks
              << '\n';

    std::cout << "Data start: "
              << superblock.dataStart
              << '\n';

    std::cout << "Free nodes: "
              << superblock.freeNodes
              << '\n';

    std::cout << "Free blocks: "
              << superblock.freeBlocks
              << '\n';

    /*
     * Read the root node directly from the node table.
     */

    std::vector<uint8_t> buffer(
        blockSize,
        0
    );

    bool readSuccess =
        device.readBlock(
            superblock.nodeTableStart,
            buffer.data()
        );

    TalonsFS::Node root{};

    if (readSuccess)
    {
        std::memcpy(
            &root,
            buffer.data(),
            sizeof(TalonsFS::Node)
        );
    }

    std::cout << "\n=== Root Node ===\n";

    std::cout << "Node table read: "
              << readSuccess
              << '\n';

    std::cout << "Root type: "
              << static_cast<int>(root.type)
              << '\n';

    std::cout << "Root size: "
              << root.size
              << '\n';

    std::cout << "Root data blocks: "
              << root.dataBlocks
              << '\n';

    /*
     * Basic validation.
     */

    bool valid = true;

    if (!formatted)
    {
        std::cout << "\nERROR: format() failed.\n";
        valid = false;
    }

    if (superblock.rootNode != TalonsFS::RootNode)
    {
        std::cout << "ERROR: incorrect root node.\n";
        valid = false;
    }

    if (root.type != TalonsFS::NodeType::Directory)
    {
        std::cout << "ERROR: root node is not a directory.\n";
        valid = false;
    }

    if (superblock.nodeTableStart !=
        layout.nodeTableStart)
    {
        std::cout << "ERROR: superblock node table location mismatch.\n";
        valid = false;
    }

    if (superblock.dataStart !=
        layout.dataStart)
    {
        std::cout << "ERROR: superblock data start mismatch.\n";
        valid = false;
    }

    std::cout << "\n=== Result ===\n";

    if (valid)
    {
        std::cout << "TalonsFS formatter test PASSED.\n";
    }
    else
    {
        std::cout << "TalonsFS formatter test FAILED.\n";
    }

    return valid ? 0 : 1;
}
