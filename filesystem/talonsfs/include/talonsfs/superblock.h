#pragma once

#include <cstdint>

namespace TalonsFS {

struct Superblock {
    uint64_t magic;
    uint32_t version;

    uint32_t blockSize;
    uint64_t totalBlocks;

    uint64_t blockBitmapStart;
    uint64_t blockBitmapBlocks;

    uint64_t nodeBitmapStart;
    uint64_t nodeBitmapBlocks;

    uint64_t nodeTableStart;
    uint64_t nodeTableBlocks;

    uint64_t nodeCount;

    uint64_t dataStart;

    uint64_t rootNode;

    uint64_t freeBlocks;
    uint64_t freeNodes;

    uint64_t filesystemID;

    uint64_t checksum;
};

}
