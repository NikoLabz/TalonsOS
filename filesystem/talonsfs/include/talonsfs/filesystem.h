#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "block_allocator.h"
#include "block_device.h"
#include "data_block_manager.h"
#include "directory.h"
#include "file_data_manager.h"
#include "layout.h"
#include "node_allocator.h"
#include "node_data_manager.h"
#include "node_manager.h"
#include "node_table.h"
#include "superblock.h"

namespace TalonsFS {

class FileSystem {
public:
    explicit FileSystem(BlockDevice& device);

    bool format();
    bool mount();
    bool sync();
    bool unmount();

    bool isMounted() const;

    bool createDirectory(
        const std::string& path
    );

    bool createFile(
        const std::string& path
    );

    bool writeFile(
        const std::string& path,
        const std::vector<uint8_t>& data
    );

    bool readFile(
        const std::string& path,
        std::vector<uint8_t>& data
    ) const;

    bool exists(
        const std::string& path
    ) const;

    bool getNode(
    const std::string& path,
    Node& node
) const;

    std::vector<DirectoryEntry> listDirectory(
        const std::string& path
    ) const;

    const Superblock& superblock() const;

private:
    bool loadMetadata();
    bool saveMetadata();

    void updateFreeCounts();

    bool resolvePath(
        const std::string& path,
        uint64_t& nodeIndex
    ) const;

    bool resolveParent(
        const std::string& path,
        uint64_t& parentNode,
        std::string& name
    ) const;

    static std::vector<std::string> splitPath(
        const std::string& path
    );

    BlockDevice& device_;

    FilesystemLayout layout_;
    Superblock superblock_;

    std::vector<uint8_t> blockBitmapStorage_;
    std::vector<uint8_t> nodeBitmapStorage_;
    std::vector<uint8_t> nodeTableStorage_;

    Bitmap blockBitmap_;
    Bitmap nodeBitmap_;

    BlockAllocator blockAllocator_;
    NodeAllocator nodeAllocator_;

    NodeTable nodeTable_;
    NodeManager nodeManager_;
    NodeDataManager nodeDataManager_;

    DataBlockManager dataBlocks_;
    Directory directory_;
    FileDataManager fileData_;

    bool mounted_;
};

}
