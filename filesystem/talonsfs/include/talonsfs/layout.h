#pragma once

#include <cstdint>

namespace TalonsFS {

constexpr uint64_t RootNode = 0;

struct FilesystemLayout {
    uint64_t superblockBlock;

    uint64_t blockBitmapStart;
    uint64_t blockBitmapBlocks;

    uint64_t nodeBitmapStart;
    uint64_t nodeBitmapBlocks;

    uint64_t nodeCount;
    uint64_t rootNode;


    uint64_t nodeTableStart;
    uint64_t nodeTableBlocks;

    uint64_t dataStart;
};

FilesystemLayout calculateLayout(
    uint64_t totalBlocks,
    uint32_t blockSize
);

}
