#include <iostream>

#include "../include/talonsfs/layout.h"
#include "../include/talonsfs/superblock.h"
#include "../include/talonsfs/superblock_utils.h"

	int main()
	{
    using namespace TalonsFS;

    Superblock superblock{};

    superblock.magic = 0x54414C4F4E534653ULL;
    superblock.version = 1;
    superblock.blockSize = 4096;
    superblock.totalBlocks = 4096;
    superblock.blockBitmapStart = 2;
    superblock.blockBitmapBlocks = 1;
    superblock.nodeBitmapStart = 3;
    superblock.nodeBitmapBlocks = 1;
    superblock.nodeTableStart = 4;
    superblock.nodeTableBlocks = 20;
    superblock.nodeCount = 1024;
    superblock.dataStart = 24;
    superblock.rootNode = RootNode;
    superblock.freeBlocks = 4072;
    superblock.freeNodes = 1023;
    superblock.filesystemID = 0x54414C4F4E534653ULL;

    superblock.checksum =
        calculateSuperblockChecksum(superblock);

    bool passed = true;

    if (!validateSuperblock(superblock)) {
        std::cout << "FAIL: freshly checksummed superblock rejected\n";
        passed = false;
    } else {
        std::cout << "PASS: freshly checksummed superblock accepted\n";
    }

    superblock.freeBlocks--;

    if (validateSuperblock(superblock)) {
        std::cout << "FAIL: modified superblock accepted with stale checksum\n";
        passed = false;
    } else {
        std::cout << "PASS: modified superblock rejected with stale checksum\n";
    }

    superblock.checksum =
        calculateSuperblockChecksum(superblock);

    if (!validateSuperblock(superblock)) {
        std::cout << "FAIL: recalculated checksum rejected\n";
        passed = false;
    } else {
        std::cout << "PASS: recalculated checksum accepted\n";
    }

    return passed ? 0 : 1;
}
