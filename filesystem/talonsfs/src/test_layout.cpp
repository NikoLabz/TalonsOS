#include <iostream>

#include "../include/talonsfs/layout.h"

int main()
{
    TalonsFS::FilesystemLayout layout =
        TalonsFS::calculateLayout(262144, 4096);

    std::cout << "Superblock: "
              << layout.superblockBlock << '\n';

    std::cout << "Block bitmap: "
              << layout.blockBitmapStart
              << " (" << layout.blockBitmapBlocks
              << " blocks)\n";

    std::cout << "Node bitmap: "
              << layout.nodeBitmapStart
              << " (" << layout.nodeBitmapBlocks
              << " blocks)\n";

    std::cout << "Node count: "
              << layout.nodeCount
              << '\n';

    std::cout << "Root node: "
          << layout.rootNode
          << '\n';

    std::cout << "Node table: "
              << layout.nodeTableStart
              << " (" << layout.nodeTableBlocks
              << " blocks)\n";

    std::cout << "Data: "
              << layout.dataStart << '\n';

    return 0;
}
