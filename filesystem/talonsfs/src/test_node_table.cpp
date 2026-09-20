#include <iostream>
#include <cstdint>

#include "../include/talonsfs/node_table.h"

int main()
{
    constexpr uint64_t nodeCount = 4;

    uint8_t data[nodeCount * sizeof(TalonsFS::Node)] = {};

    TalonsFS::NodeTable table(
        data,
        nodeCount
    );

    TalonsFS::Node root{};

    root.type = TalonsFS::NodeType::Directory;
    root.size = 1234;
    root.dataStart = 500;
    root.dataBlocks = 2;

    table.write(0, root);

    TalonsFS::Node loaded =
        table.read(0);

    std::cout << "Type: "
              << static_cast<int>(loaded.type)
              << '\n';

    std::cout << "Size: "
              << loaded.size
              << '\n';

    std::cout << "Data start: "
              << loaded.dataStart
              << '\n';

    std::cout << "Data blocks: "
              << loaded.dataBlocks
              << '\n';

    return 0;
}
