#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"
#include "../include/talonsfs/node.h"
#include "../include/talonsfs/node_allocator.h"
#include "../include/talonsfs/node_manager.h"
#include "../include/talonsfs/node_table.h"
#include "../include/talonsfs/permissions.h"

int main()
{
    /*
     * Newly-created node has explicit full permissions
     */

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

    const int64_t fileNode =
        manager.create(
            TalonsFS::NodeType::File
        );

    if (fileNode < 0) {
        std::cout
            << "Newly-created node has explicit full permissions: FAILED\n";

        return 1;
    }

    TalonsFS::Node created{};

    if (!manager.read(
            static_cast<uint64_t>(fileNode),
            created))
    {
        std::cout
            << "Newly-created node has explicit full permissions: FAILED\n";

        return 1;
    }

    if (created.flags != TalonsFS::PERMISSIONS_FULL ||
        !TalonsFS::hasReadPermission(created) ||
        !TalonsFS::hasWritePermission(created) ||
        !TalonsFS::hasExecutePermission(created))
    {
        std::cout
            << "Newly-created node has explicit full permissions: FAILED\n";

        return 1;
    }

    std::cout
        << "Newly-created node has explicit full permissions: PASSED\n";

    /*
     * Legacy node (flags == 0) is treated as full rwx
     */

    TalonsFS::Node legacy{};
    legacy.type = TalonsFS::NodeType::File;
    legacy.flags = 0;

    if (!TalonsFS::hasReadPermission(legacy)) {
        std::cout
            << "Legacy flags == 0 has read permission: FAILED\n";

        return 1;
    }

    std::cout
        << "Legacy flags == 0 has read permission: PASSED\n";

    if (!TalonsFS::hasWritePermission(legacy)) {
        std::cout
            << "Legacy flags == 0 has write permission: FAILED\n";

        return 1;
    }

    std::cout
        << "Legacy flags == 0 has write permission: PASSED\n";

    if (!TalonsFS::hasExecutePermission(legacy)) {
        std::cout
            << "Legacy flags == 0 has execute permission: FAILED\n";

        return 1;
    }

    std::cout
        << "Legacy flags == 0 has execute permission: PASSED\n";

    /*
     * Explicit permission grants/denials
     */

    TalonsFS::Node readOnly{};
    readOnly.type = TalonsFS::NodeType::File;
    readOnly.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_READ;

    if (!TalonsFS::hasReadPermission(readOnly)) {
        std::cout
            << "Explicit read permission works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit read permission works: PASSED\n";

    TalonsFS::Node noRead{};
    noRead.type = TalonsFS::NodeType::File;
    noRead.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_WRITE |
        TalonsFS::PERMISSION_EXECUTE;

    if (TalonsFS::hasReadPermission(noRead)) {
        std::cout
            << "Explicit read denial works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit read denial works: PASSED\n";

    TalonsFS::Node writeOnly{};
    writeOnly.type = TalonsFS::NodeType::File;
    writeOnly.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_WRITE;

    if (!TalonsFS::hasWritePermission(writeOnly)) {
        std::cout
            << "Explicit write permission works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit write permission works: PASSED\n";

    TalonsFS::Node noWrite{};
    noWrite.type = TalonsFS::NodeType::File;
    noWrite.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_READ |
        TalonsFS::PERMISSION_EXECUTE;

    if (TalonsFS::hasWritePermission(noWrite)) {
        std::cout
            << "Explicit write denial works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit write denial works: PASSED\n";

    TalonsFS::Node executeOnly{};
    executeOnly.type = TalonsFS::NodeType::File;
    executeOnly.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_EXECUTE;

    if (!TalonsFS::hasExecutePermission(executeOnly)) {
        std::cout
            << "Explicit execute permission works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit execute permission works: PASSED\n";

    TalonsFS::Node noExecute{};
    noExecute.type = TalonsFS::NodeType::File;
    noExecute.flags =
        TalonsFS::PERMISSIONS_INITIALIZED |
        TalonsFS::PERMISSION_READ |
        TalonsFS::PERMISSION_WRITE;

    if (TalonsFS::hasExecutePermission(noExecute)) {
        std::cout
            << "Explicit execute denial works: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit execute denial works: PASSED\n";

    /*
     * Genuine chmod 000: PERMISSIONS_INITIALIZED set, no rwx bits.
     * Must be distinguishable from the legacy flags == 0 state and
     * must deny all three permissions.
     */

    TalonsFS::Node noPermissions{};
    noPermissions.type = TalonsFS::NodeType::File;
    noPermissions.flags =
        TalonsFS::PERMISSIONS_INITIALIZED;

    if (noPermissions.flags != 0b1000) {
        std::cout
            << "Explicit 000 denies all permissions: FAILED\n";

        return 1;
    }

    if (TalonsFS::hasReadPermission(noPermissions) ||
        TalonsFS::hasWritePermission(noPermissions) ||
        TalonsFS::hasExecutePermission(noPermissions))
    {
        std::cout
            << "Explicit 000 denies all permissions: FAILED\n";

        return 1;
    }

    std::cout
        << "Explicit 000 denies all permissions: PASSED\n";

    /*
     * Reserved bits (4-7) must not affect permission checks.
     */

    TalonsFS::Node reservedBitsSet{};
    reservedBitsSet.type = TalonsFS::NodeType::File;
    reservedBitsSet.flags =
        static_cast<uint8_t>(
            TalonsFS::PERMISSIONS_INITIALIZED |
            TalonsFS::PERMISSION_READ |
            0xF0
        );

    if (!TalonsFS::hasReadPermission(reservedBitsSet) ||
        TalonsFS::hasWritePermission(reservedBitsSet) ||
        TalonsFS::hasExecutePermission(reservedBitsSet))
    {
        std::cout
            << "Reserved bits do not affect permission checks: FAILED\n";

        return 1;
    }

    std::cout
        << "Reserved bits do not affect permission checks: PASSED\n";

    /*
     * All 8 combinations of bits 0-2 behave correctly once
     * PERMISSIONS_INITIALIZED is set.
     */

    bool allCombinationsPassed = true;

    for (uint8_t bits = 0; bits < 8; ++bits) {

        TalonsFS::Node combo{};
        combo.type = TalonsFS::NodeType::File;
        combo.flags =
            static_cast<uint8_t>(
                TalonsFS::PERMISSIONS_INITIALIZED | bits
            );

        const bool expectRead =
            (bits & TalonsFS::PERMISSION_READ) != 0;

        const bool expectWrite =
            (bits & TalonsFS::PERMISSION_WRITE) != 0;

        const bool expectExecute =
            (bits & TalonsFS::PERMISSION_EXECUTE) != 0;

        if (TalonsFS::hasReadPermission(combo) != expectRead ||
            TalonsFS::hasWritePermission(combo) != expectWrite ||
            TalonsFS::hasExecutePermission(combo) != expectExecute)
        {
            allCombinationsPassed = false;
            break;
        }
    }

    if (!allCombinationsPassed) {
        std::cout
            << "All bit 0-2 combinations behave correctly: FAILED\n";

        return 1;
    }

    std::cout
        << "All bit 0-2 combinations behave correctly: PASSED\n";

    return 0;
}
