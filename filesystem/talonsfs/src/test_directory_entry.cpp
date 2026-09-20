#include <iostream>
#include <cstring>

#include "../include/talonsfs/directory_entry.h"

int main()
{
    TalonsFS::DirectoryEntry entry{};

    entry.node = 1;
    entry.type = 1;

    std::strncpy(
        entry.name,
        "hello.txt",
        TalonsFS::MaxFilenameLength - 1
    );

    std::cout << "Node: "
              << entry.node
              << '\n';

    std::cout << "Type: "
              << static_cast<int>(entry.type)
              << '\n';

    std::cout << "Name: "
              << entry.name
              << '\n';

    std::cout << "Entry size: "
              << sizeof(entry)
              << " bytes\n";

    return 0;
}
