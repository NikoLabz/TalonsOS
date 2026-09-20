// NOTE: host-only. Uses <vector>/<algorithm>/<limits> or other hosted-C++/libc
// features and cannot compile in the freestanding kernel build. Used by the
// TalonsFS test suite and host-side tools (e.g. mkTalonsFS) only — never
// #include this from kernel/ code.

#include <algorithm>
#include <cstring>
#include <cstdio>

#include "../include/talonsfs/filesystem.h"
#include "../include/talonsfs/formatter.h"

namespace TalonsFS {

namespace {

constexpr uint64_t SuperblockBlock = 1;
constexpr uint64_t SuperblockMagic =
    0x54414C4F4E534653ULL;
constexpr uint32_t FilesystemVersion = 1;

}

FileSystem::FileSystem(BlockDevice& device)
    : device_(device),
      layout_(
          calculateLayout(
              device.getBlockCount(),
              device.getBlockSize()
          )
      ),
      superblock_{},
      blockBitmapStorage_(
          (device.getBlockCount() + 7) / 8,
          0
      ),
      nodeBitmapStorage_(
          (layout_.nodeCount + 7) / 8,
          0
      ),
      nodeTableStorage_(
          layout_.nodeCount * sizeof(Node),
          0
      ),
      blockBitmap_(
          blockBitmapStorage_.data(),
          device.getBlockCount()
      ),
      nodeBitmap_(
          nodeBitmapStorage_.data(),
          layout_.nodeCount
      ),
      blockAllocator_(
          blockBitmap_,
          device.getBlockCount(),
          layout_.dataStart
      ),
      nodeAllocator_(
          nodeBitmap_,
          nodeTable_
      ),
      nodeTable_(
          nodeTableStorage_.data(),
          layout_.nodeCount
      ),
      nodeManager_(
          nodeAllocator_,
          nodeTable_
      ),
      nodeDataManager_(
          blockAllocator_,
          nodeTable_
      ),
      dataBlocks_(
          device_
      ),
      directory_(
          dataBlocks_,
          nodeTable_,
          nodeDataManager_
      ),
      fileData_(
          dataBlocks_,
          nodeTable_,
          layout_.dataStart,
          device.getBlockCount()
      ),
      mounted_(false)
{
}

bool FileSystem::format()
{
    if (!TalonsFS::format(
            device_,
            superblock_
        ))
    {
        return false;
    }

    mounted_ = false;

    return mount();
}

bool FileSystem::mount()
{
    printf("MOUNT: starting\n");

    if (device_.getBlockSize() == 0 ||
        device_.getBlockCount() <= SuperblockBlock)
    {
        printf("MOUNT: invalid device geometry\n");
        return false;
    }

    printf(
        "MOUNT: device blocks=%llu blockSize=%u\n",
        static_cast<unsigned long long>(
            device_.getBlockCount()
        ),
        device_.getBlockSize()
    );

    printf(
        "MOUNT: reading superblock block %llu\n",
        static_cast<unsigned long long>(
            SuperblockBlock
        )
    );

    std::vector<uint8_t> buffer(
        device_.getBlockSize(),
        0
    );

    if (!device_.readBlock(
            SuperblockBlock,
            buffer.data()
        ))
    {
        printf("MOUNT: failed to read superblock\n");
        return false;
    }

    Superblock loaded{};

    std::memcpy(
        &loaded,
        buffer.data(),
        sizeof(Superblock)
    );

    printf(
        "MOUNT: magic=0x%llx\n",
        static_cast<unsigned long long>(
            loaded.magic
        )
    );

    printf(
        "MOUNT: version=%u\n",
        loaded.version
    );

    printf(
        "MOUNT: blockSize=%u\n",
        loaded.blockSize
    );

    printf(
        "MOUNT: totalBlocks=%llu\n",
        static_cast<unsigned long long>(
            loaded.totalBlocks
        )
    );

    printf(
        "MOUNT: blockBitmapStart=%llu\n",
        static_cast<unsigned long long>(
            loaded.blockBitmapStart
        )
    );

    printf(
        "MOUNT: blockBitmapBlocks=%llu\n",
        static_cast<unsigned long long>(
            loaded.blockBitmapBlocks
        )
    );

    printf(
        "MOUNT: nodeBitmapStart=%llu\n",
        static_cast<unsigned long long>(
            loaded.nodeBitmapStart
        )
    );

    printf(
        "MOUNT: nodeBitmapBlocks=%llu\n",
        static_cast<unsigned long long>(
            loaded.nodeBitmapBlocks
        )
    );

    printf(
        "MOUNT: nodeTableStart=%llu\n",
        static_cast<unsigned long long>(
            loaded.nodeTableStart
        )
    );

    printf(
        "MOUNT: nodeTableBlocks=%llu\n",
        static_cast<unsigned long long>(
            loaded.nodeTableBlocks
        )
    );

    printf(
        "MOUNT: nodeCount=%llu\n",
        static_cast<unsigned long long>(
            loaded.nodeCount
        )
    );

    printf(
        "MOUNT: dataStart=%llu\n",
        static_cast<unsigned long long>(
            loaded.dataStart
        )
    );

    printf(
        "MOUNT: rootNode=%llu\n",
        static_cast<unsigned long long>(
            loaded.rootNode
        )
    );

    /*
     * -------------------------
     * Superblock validation
     * -------------------------
     */

    if (loaded.magic != SuperblockMagic) {
        printf("MOUNT FAIL: bad magic\n");
        return false;
    }

    printf("MOUNT: magic OK\n");

    if (loaded.version != FilesystemVersion) {
        printf("MOUNT FAIL: bad version\n");
        return false;
    }

    printf("MOUNT: version OK\n");

    if (loaded.blockSize !=
        device_.getBlockSize())
    {
        printf("MOUNT FAIL: bad block size\n");
        return false;
    }

    printf("MOUNT: block size OK\n");

    if (loaded.totalBlocks !=
        device_.getBlockCount())
    {
        printf("MOUNT FAIL: bad total blocks\n");
        return false;
    }

    printf("MOUNT: total blocks OK\n");

    FilesystemLayout expected =
        calculateLayout(
            loaded.totalBlocks,
            loaded.blockSize
        );

    printf(
        "MOUNT: expected blockBitmapStart=%llu\n",
        static_cast<unsigned long long>(
            expected.blockBitmapStart
        )
    );

    printf(
        "MOUNT: expected blockBitmapBlocks=%llu\n",
        static_cast<unsigned long long>(
            expected.blockBitmapBlocks
        )
    );

    printf(
        "MOUNT: expected nodeBitmapStart=%llu\n",
        static_cast<unsigned long long>(
            expected.nodeBitmapStart
        )
    );

    printf(
        "MOUNT: expected nodeBitmapBlocks=%llu\n",
        static_cast<unsigned long long>(
            expected.nodeBitmapBlocks
        )
    );

    printf(
        "MOUNT: expected nodeTableStart=%llu\n",
        static_cast<unsigned long long>(
            expected.nodeTableStart
        )
    );

    printf(
        "MOUNT: expected nodeTableBlocks=%llu\n",
        static_cast<unsigned long long>(
            expected.nodeTableBlocks
        )
    );

    printf(
        "MOUNT: expected nodeCount=%llu\n",
        static_cast<unsigned long long>(
            expected.nodeCount
        )
    );

    printf(
        "MOUNT: expected dataStart=%llu\n",
        static_cast<unsigned long long>(
            expected.dataStart
        )
    );

    if (loaded.blockBitmapStart !=
        expected.blockBitmapStart)
    {
        printf("MOUNT FAIL: bad block bitmap start\n");
        return false;
    }

    printf("MOUNT: block bitmap start OK\n");

    if (loaded.blockBitmapBlocks !=
        expected.blockBitmapBlocks)
    {
        printf("MOUNT FAIL: bad block bitmap block count\n");
        return false;
    }

    printf("MOUNT: block bitmap blocks OK\n");

    if (loaded.nodeBitmapStart !=
        expected.nodeBitmapStart)
    {
        printf("MOUNT FAIL: bad node bitmap start\n");
        return false;
    }

    printf("MOUNT: node bitmap start OK\n");

    if (loaded.nodeBitmapBlocks !=
        expected.nodeBitmapBlocks)
    {
        printf("MOUNT FAIL: bad node bitmap block count\n");
        return false;
    }

    printf("MOUNT: node bitmap blocks OK\n");

    if (loaded.nodeTableStart !=
        expected.nodeTableStart)
    {
        printf("MOUNT FAIL: bad node table start\n");
        return false;
    }

    printf("MOUNT: node table start OK\n");

    if (loaded.nodeTableBlocks !=
        expected.nodeTableBlocks)
    {
        printf("MOUNT FAIL: bad node table block count\n");
        return false;
    }

    printf("MOUNT: node table blocks OK\n");

    if (loaded.nodeCount !=
        expected.nodeCount)
    {
        printf("MOUNT FAIL: bad node count\n");
        return false;
    }

    printf("MOUNT: node count OK\n");

    if (loaded.dataStart !=
        expected.dataStart)
    {
        printf("MOUNT FAIL: bad data start\n");
        return false;
    }

    printf("MOUNT: data start OK\n");

    if (loaded.rootNode != RootNode) {
        printf("MOUNT FAIL: bad root node\n");
        return false;
    }

    printf("MOUNT: root node index OK\n");

    /*
     * -------------------------
     * Load metadata
     * -------------------------
     */

    superblock_ = loaded;
    layout_ = expected;

    printf("MOUNT: loading metadata\n");

    if (!loadMetadata()) {
        printf("MOUNT FAIL: loadMetadata()\n");
        return false;
    }

    printf("MOUNT: metadata loaded\n");

    /*
     * -------------------------
     * Verify root node bitmap
     * -------------------------
     */

    if (!nodeBitmap_.get(RootNode)) {
        printf("MOUNT FAIL: root node not allocated\n");
        return false;
    }

    printf("MOUNT: root node bitmap OK\n");

    /*
     * -------------------------
     * Verify root node
     * -------------------------
     */

    Node root =
        nodeTable_.read(RootNode);

    printf(
        "MOUNT: root type=%u\n",
        static_cast<unsigned int>(root.type)
    );

    printf(
        "MOUNT: root size=%llu\n",
        static_cast<unsigned long long>(root.size)
    );

    printf(
        "MOUNT: root dataStart=%llu\n",
        static_cast<unsigned long long>(
            root.dataStart
        )
    );

    printf(
        "MOUNT: root dataBlocks=%llu\n",
        static_cast<unsigned long long>(
            root.dataBlocks
        )
    );

    if (root.type != NodeType::Directory) {
        printf("MOUNT FAIL: root node is not a directory\n");
        return false;
    }

    printf("MOUNT: root node is a directory\n");

    mounted_ = true;

    printf("MOUNT: SUCCESS\n");

    return true;
}

bool FileSystem::sync()
{
    if (!mounted_) {
        return false;
    }

    updateFreeCounts();

    return saveMetadata();
}

bool FileSystem::unmount()
{
    if (!mounted_) {
        return false;
    }

    if (!sync()) {
        return false;
    }

    mounted_ = false;

    return true;
}

bool FileSystem::isMounted() const
{
    return mounted_;
}

bool FileSystem::loadMetadata()
{
    const uint32_t blockSize =
        device_.getBlockSize();

    /*
     * Block bitmap
     */

    for (uint64_t i = 0;
         i < layout_.blockBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        if (!device_.readBlock(
                layout_.blockBitmapStart + i,
                buffer.data()
            ))
        {
            return false;
        }

        const uint64_t offset =
            i * blockSize;

        if (offset >=
            blockBitmapStorage_.size())
        {
            break;
        }

        const uint64_t remaining =
            blockBitmapStorage_.size() -
            offset;

        const uint64_t bytesToCopy =
            std::min<uint64_t>(
                remaining,
                blockSize
            );

        std::memcpy(
            blockBitmapStorage_.data() + offset,
            buffer.data(),
            bytesToCopy
        );
    }

    /*
     * Node bitmap
     */

    for (uint64_t i = 0;
         i < layout_.nodeBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        if (!device_.readBlock(
                layout_.nodeBitmapStart + i,
                buffer.data()
            ))
        {
            return false;
        }

        const uint64_t offset =
            i * blockSize;

        if (offset >=
            nodeBitmapStorage_.size())
        {
            break;
        }

        const uint64_t remaining =
            nodeBitmapStorage_.size() -
            offset;

        const uint64_t bytesToCopy =
            std::min<uint64_t>(
                remaining,
                blockSize
            );

        std::memcpy(
            nodeBitmapStorage_.data() + offset,
            buffer.data(),
            bytesToCopy
        );
    }

    /*
     * Node table
     */

    for (uint64_t i = 0;
         i < layout_.nodeTableBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        if (!device_.readBlock(
                layout_.nodeTableStart + i,
                buffer.data()
            ))
        {
            return false;
        }

        const uint64_t offset =
            i * blockSize;

        if (offset >=
            nodeTableStorage_.size())
        {
            break;
        }

        const uint64_t remaining =
            nodeTableStorage_.size() -
            offset;

        const uint64_t bytesToCopy =
            std::min<uint64_t>(
                remaining,
                blockSize
            );

        std::memcpy(
            nodeTableStorage_.data() + offset,
            buffer.data(),
            bytesToCopy
        );
    }

    return true;
}

bool FileSystem::saveMetadata()
{
    const uint32_t blockSize =
        device_.getBlockSize();

    /*
     * Block bitmap
     */

    for (uint64_t i = 0;
         i < layout_.blockBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        const uint64_t offset =
            i * blockSize;

        if (offset <
            blockBitmapStorage_.size())
        {
            const uint64_t remaining =
                blockBitmapStorage_.size() -
                offset;

            const uint64_t bytesToCopy =
                std::min<uint64_t>(
                    remaining,
                    blockSize
                );

            std::memcpy(
                buffer.data(),
                blockBitmapStorage_.data() + offset,
                bytesToCopy
            );
        }

        if (!device_.writeBlock(
                layout_.blockBitmapStart + i,
                buffer.data()
            ))
        {
            return false;
        }
    }

    /*
     * Node bitmap
     */

    for (uint64_t i = 0;
         i < layout_.nodeBitmapBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        const uint64_t offset =
            i * blockSize;

        if (offset <
            nodeBitmapStorage_.size())
        {
            const uint64_t remaining =
                nodeBitmapStorage_.size() -
                offset;

            const uint64_t bytesToCopy =
                std::min<uint64_t>(
                    remaining,
                    blockSize
                );

            std::memcpy(
                buffer.data(),
                nodeBitmapStorage_.data() + offset,
                bytesToCopy
            );
        }

        if (!device_.writeBlock(
                layout_.nodeBitmapStart + i,
                buffer.data()
            ))
        {
            return false;
        }
    }

    /*
     * Node table
     */

    for (uint64_t i = 0;
         i < layout_.nodeTableBlocks;
         ++i)
    {
        std::vector<uint8_t> buffer(
            blockSize,
            0
        );

        const uint64_t offset =
            i * blockSize;

        if (offset <
            nodeTableStorage_.size())
        {
            const uint64_t remaining =
                nodeTableStorage_.size() -
                offset;

            const uint64_t bytesToCopy =
                std::min<uint64_t>(
                    remaining,
                    blockSize
                );

            std::memcpy(
                buffer.data(),
                nodeTableStorage_.data() + offset,
                bytesToCopy
            );
        }

        if (!device_.writeBlock(
                layout_.nodeTableStart + i,
                buffer.data()
            ))
        {
            return false;
        }
    }

    /*
     * Superblock
     */

    std::vector<uint8_t> superblockBuffer(
        blockSize,
        0
    );

    std::memcpy(
        superblockBuffer.data(),
        &superblock_,
        sizeof(Superblock)
    );

    return device_.writeBlock(
        SuperblockBlock,
        superblockBuffer.data()
    );
}

void FileSystem::updateFreeCounts()
{
    uint64_t freeBlocks = 0;

    for (uint64_t block = layout_.dataStart;
         block < device_.getBlockCount();
         ++block)
    {
        if (!blockBitmap_.get(block)) {
            ++freeBlocks;
        }
    }

    uint64_t freeNodes = 0;

    for (uint64_t node = 0;
         node < layout_.nodeCount;
         ++node)
    {
        if (!nodeBitmap_.get(node)) {
            ++freeNodes;
        }
    }

    superblock_.freeBlocks =
        freeBlocks;

    superblock_.freeNodes =
        freeNodes;
}

std::vector<std::string> FileSystem::splitPath(
    const std::string& path
)
{
    std::vector<std::string> parts;

    if (path.empty() ||
        path.front() != '/')
    {
        return parts;
    }

    std::size_t start = 1;

    while (start < path.size()) {
        while (start < path.size() &&
               path[start] == '/')
        {
            ++start;
        }

        if (start >= path.size()) {
            break;
        }

        const std::size_t end =
            path.find('/', start);

        if (end == std::string::npos) {
            parts.push_back(
                path.substr(start)
            );

            break;
        }

        parts.push_back(
            path.substr(
                start,
                end - start
            )
        );

        start = end + 1;
    }

    return parts;
}

bool FileSystem::resolvePath(
    const std::string& path,
    uint64_t& nodeIndex
) const
{
    if (!mounted_) {
        return false;
    }

    if (path == "/") {
        nodeIndex = RootNode;
        return true;
    }

    const std::vector<std::string> parts =
        splitPath(path);

    if (parts.empty()) {
        return false;
    }

    uint64_t current = RootNode;

    for (const std::string& part : parts) {
        DirectoryEntry entry{};

        if (!directory_.find(
                current,
                part.c_str(),
                entry
            ))
        {
            return false;
        }

        current = entry.node;
    }

    nodeIndex = current;
    return true;
}

bool FileSystem::resolveParent(
    const std::string& path,
    uint64_t& parentNode,
    std::string& name
) const
{
    if (!mounted_) {
        return false;
    }

    const std::vector<std::string> parts =
        splitPath(path);

    if (parts.empty()) {
        return false;
    }

    name = parts.back();

    if (name.empty() ||
        name.size() >= MaxFilenameLength)
    {
        return false;
    }

    parentNode = RootNode;

    for (std::size_t i = 0;
         i + 1 < parts.size();
         ++i)
    {
        DirectoryEntry entry{};

        if (!directory_.find(
                parentNode,
                parts[i].c_str(),
                entry
            ))
        {
            return false;
        }

        Node parent =
            nodeTable_.read(entry.node);

        if (parent.type != NodeType::Directory) {
            return false;
        }

        parentNode = entry.node;
    }

    return true;
}

bool FileSystem::createDirectory(
    const std::string& path
)
{
    if (!mounted_) {
        return false;
    }

    uint64_t parentNode = 0;
    std::string name;

    if (!resolveParent(
            path,
            parentNode,
            name
        ))
    {
        return false;
    }

    DirectoryEntry existing{};

    if (directory_.find(
            parentNode,
            name.c_str(),
            existing
        ))
    {
        return false;
    }

    const int64_t node =
        nodeManager_.create(
            NodeType::Directory
        );

    if (node < 0) {
        return false;
    }

    DirectoryEntry entry{};

    entry.node =
        static_cast<uint64_t>(node);

    entry.type =
        static_cast<uint8_t>(
            NodeType::Directory
        );

    std::strncpy(
        entry.name,
        name.c_str(),
        MaxFilenameLength - 1
    );

    if (!directory_.add(
            parentNode,
            entry
        ))
    {
        nodeManager_.remove(
            static_cast<uint64_t>(node)
        );

        return false;
    }

    return sync();
}

bool FileSystem::createFile(
    const std::string& path
)
{
    if (!mounted_) {
        return false;
    }

    uint64_t parentNode = 0;
    std::string name;

    if (!resolveParent(
            path,
            parentNode,
            name
        ))
    {
        return false;
    }

    DirectoryEntry existing{};

    if (directory_.find(
            parentNode,
            name.c_str(),
            existing
        ))
    {
        return false;
    }

    const int64_t node =
        nodeManager_.create(
            NodeType::File
        );

    if (node < 0) {
        return false;
    }

    DirectoryEntry entry{};

    entry.node =
        static_cast<uint64_t>(node);

    entry.type =
        static_cast<uint8_t>(
            NodeType::File
        );

    std::strncpy(
        entry.name,
        name.c_str(),
        MaxFilenameLength - 1
    );

    if (!directory_.add(
            parentNode,
            entry
        ))
    {
        nodeManager_.remove(
            static_cast<uint64_t>(node)
        );

        return false;
    }

    return sync();
}

bool FileSystem::writeFile(
    const std::string& path,
    const std::vector<uint8_t>& data
)
{
    if (!mounted_) {
        return false;
    }

    uint64_t nodeIndex = 0;

    if (!resolvePath(
            path,
            nodeIndex
        ))
    {
        return false;
    }

    Node node =
        nodeTable_.read(nodeIndex);

    if (node.type != NodeType::File) {
        return false;
    }

    const uint64_t blockSize =
        dataBlocks_.getBlockSize();

    if (blockSize == 0) {
        return false;
    }

    const uint64_t requiredBlocks =
        data.empty()
            ? 0
            : (data.size() + blockSize - 1) /
              blockSize;

    while (node.dataBlocks < requiredBlocks) {
        if (!nodeDataManager_.allocateBlock(
                nodeIndex
            ))
        {
            return false;
        }

        node =
            nodeTable_.read(nodeIndex);
    }

    if (!fileData_.write(
            nodeIndex,
            data.data(),
            data.size()
        ))
    {
        return false;
    }

    return sync();
}

bool FileSystem::readFile(
    const std::string& path,
    std::vector<uint8_t>& data
) const
{
    if (!mounted_) {
        return false;
    }

    uint64_t nodeIndex = 0;

    if (!resolvePath(
            path,
            nodeIndex
        ))
    {
        return false;
    }

    Node node =
        nodeTable_.read(nodeIndex);

    if (node.type != NodeType::File) {
        return false;
    }

    data.resize(node.size);

    if (node.size == 0) {
        return true;
    }

    return fileData_.read(
        nodeIndex,
        data.data(),
        data.size()
    );
}

bool FileSystem::exists(
    const std::string& path
) const
{
    if (!mounted_) {
        return false;
    }

    uint64_t nodeIndex = 0;

    return resolvePath(
        path,
        nodeIndex
    );
}

bool FileSystem::getNode(
    const std::string& path,
    Node& node
) const
{
    uint64_t nodeIndex = 0;

    if (!resolvePath(
            path,
            nodeIndex
        ))
    {
        return false;
    }

    node = nodeTable_.read(nodeIndex);

    return true;
}

std::vector<DirectoryEntry> FileSystem::listDirectory(
    const std::string& path
) const
{
    std::vector<DirectoryEntry> entries;

    if (!mounted_) {
        return entries;
    }

    uint64_t nodeIndex = 0;

    if (!resolvePath(
            path,
            nodeIndex
        ))
    {
        return entries;
    }

    Node node =
        nodeTable_.read(nodeIndex);

    if (node.type != NodeType::Directory) {
        return entries;
    }

    return directory_.list(nodeIndex);
}

const Superblock& FileSystem::superblock() const
{
    return superblock_;
}

} // namespace TalonsFS
