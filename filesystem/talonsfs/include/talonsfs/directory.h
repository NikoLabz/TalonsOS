#pragma once

#include <cstdint>
#include <vector>

#include "data_block_manager.h"
#include "directory_entry.h"
#include "node_data_manager.h"
#include "node_table.h"

namespace TalonsFS {

class Directory {
public:

    Directory(
        DataBlockManager& dataBlocks,
        NodeTable& nodeTable,
        NodeDataManager& nodeData
    );


    bool add(
        uint64_t directoryNode,
        const DirectoryEntry& entry
    );


    bool find(
        uint64_t directoryNode,
        const char* name,
        DirectoryEntry& result
    ) const;


    std::vector<DirectoryEntry> list(
        uint64_t directoryNode
    ) const;


private:

    DataBlockManager& dataBlocks_;
    NodeTable& nodeTable_;
    NodeDataManager& nodeData_;

};

}
