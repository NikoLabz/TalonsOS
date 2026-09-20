#include <iostream>
#include "../include/talonsfs/superblock.h"

int main()
{
    TalonsFS::Superblock sb{};

    sb.magic = 0x54414C4F4E534653;

    std::cout << "Magic: 0x"
              << std::hex
              << sb.magic
              << "\n";

    return 0;
}

