#include <iostream>
#include "../include/talonsfs/superblock.h"

int main()
{
    std::cout << "Superblock size: "
              << sizeof(TalonsFS::Superblock)
              << " bytes\n";

    return 0;
}
