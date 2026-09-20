#include <iostream>
#include <cstdint>
#include <thread>
#include <chrono>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/node_allocator.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/node_manager.h"
#include "../include/talonsfs/time.h"

int main()
{
    constexpr uint64_t nodeCount = 16;

uint8_t bitmapData[2] = {};

TalonsFS::Bitmap bitmap(
    bitmapData,
    nodeCount
);

uint8_t nodeData[
    nodeCount * sizeof(TalonsFS::Node)
] = {};

TalonsFS::NodeTable table(
    nodeData,
    nodeCount
);

TalonsFS::NodeAllocator allocator(
    bitmap,
    table
);


    TalonsFS::NodeManager manager(
        allocator,
        table
    );

    int64_t fileNode =
        manager.create(
            TalonsFS::NodeType::File
        );

    std::cout << "Created node: "
              << fileNode
              << '\n';

	TalonsFS::Node created{};

if (!manager.read(
        static_cast<uint64_t>(fileNode),
        created
    ))
{
    std::cerr << "Failed to read created node\n";
    return 1;
}

std::cout << "Created time: "
          << created.createdTime
          << '\n';

std::cout << "Modified time: "
          << created.modifiedTime
          << '\n';

if (created.createdTime == 0)
{
    std::cerr << "FAIL: createdTime is unset\n";
    return 1;
}

if (created.modifiedTime == 0)
{
    std::cerr << "FAIL: modifiedTime is unset\n";
    return 1;
}

if (created.createdTime != created.modifiedTime)
{
    std::cerr << "FAIL: creation and modification timestamps differ\n";
    return 1;
}

    TalonsFS::Node file{};

    if (manager.read(
            static_cast<uint64_t>(fileNode),
            file
        ))
    {
        std::cout << "Type: "
                  << static_cast<int>(file.type)
                  << '\n';
    }

const uint64_t originalModifiedTime =
    created.modifiedTime;

while (TalonsFS::currentUnixTime() <= originalModifiedTime)
{
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );
}

file.size = 12345;

manager.write(
    static_cast<uint64_t>(fileNode),
    file
);

TalonsFS::Node modified{};

if (!manager.read(
        static_cast<uint64_t>(fileNode),
        modified
    ))
{
    std::cerr << "Failed to read modified node\n";
    return 1;
}

std::cout << "Modified time after write: "
          << modified.modifiedTime
          << '\n';

if (modified.createdTime != created.createdTime)
{
    std::cerr << "FAIL: createdTime changed\n";
    return 1;
}

if (modified.modifiedTime <= originalModifiedTime)
{
    std::cerr << "FAIL: modifiedTime did not increase\n";
    return 1;
}

    TalonsFS::Node loaded{};

    manager.read(
        static_cast<uint64_t>(fileNode),
        loaded
    );

    std::cout << "Size: "
              << loaded.size
              << '\n';

    manager.remove(
        static_cast<uint64_t>(fileNode)
    );

    TalonsFS::Node removed{};

    bool exists =
        manager.read(
            static_cast<uint64_t>(fileNode),
            removed
        );

    std::cout << "Exists after remove: "
              << exists
              << '\n';

    return 0;
}
