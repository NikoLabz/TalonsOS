#include "file_descriptor.h"

namespace
{
    FileDescriptor descriptors[
        MAX_FILE_DESCRIPTORS
    ];
}

bool file_descriptors_init()
{
    for (uint32_t i = 0;
         i < MAX_FILE_DESCRIPTORS;
         ++i)
    {
        descriptors[i].used = false;
        descriptors[i].node = 0;
        descriptors[i].offset = 0;
        descriptors[i].flags = 0;
    }

    return true;
}

int file_descriptor_allocate(
    uint64_t node,
    uint32_t flags
)
{
    for (uint32_t i = 3;
         i < MAX_FILE_DESCRIPTORS;
         ++i)
    {
        if (!descriptors[i].used)
        {
            descriptors[i].used = true;
            descriptors[i].node = node;
            descriptors[i].offset = 0;
            descriptors[i].flags = flags;

            return static_cast<int>(i);
        }
    }

    return -1;
}

bool file_descriptor_close(int fd)
{
    if (fd < 3 ||
        fd >= static_cast<int>(MAX_FILE_DESCRIPTORS))
    {
        return false;
    }

    if (!descriptors[fd].used)
    {
        return false;
    }

    descriptors[fd].used = false;
    descriptors[fd].node = 0;
    descriptors[fd].offset = 0;
    descriptors[fd].flags = 0;

    return true;
}

FileDescriptor* file_descriptor_get(int fd)
{
    if (fd < 0 ||
        fd >= static_cast<int>(MAX_FILE_DESCRIPTORS))
    {
        return nullptr;
    }

    if (!descriptors[fd].used)
    {
        return nullptr;
    }

    return &descriptors[fd];
}
