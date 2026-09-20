#include <iostream>

#include "../include/talonsfs/node.h"

int main()
{
    TalonsFS::Node root{};

    root.type = TalonsFS::NodeType::Directory;

    std::cout << "Root node: 0\n";

    std::cout << "Type: "
              << static_cast<int>(root.type)
              << '\n';

    return 0;
}
