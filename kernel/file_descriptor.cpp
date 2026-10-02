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
        descriptors[i].type = FileDescriptorType::File;
        descriptors[i].node = 0;
        descriptors[i].offset = 0;
        descriptors[i].flags = 0;
        descriptors[i].pipeId = 0;
    }

    descriptors[STDIN_FILENO].used = true;
    descriptors[STDIN_FILENO].node = 0;
    descriptors[STDIN_FILENO].offset = 0;
    descriptors[STDIN_FILENO].flags = 0;
    descriptors[STDIN_FILENO].type = FileDescriptorType::File;
    descriptors[STDIN_FILENO].pipeId = 0;

    descriptors[STDOUT_FILENO].used = true;
    descriptors[STDOUT_FILENO].node = 0;
    descriptors[STDOUT_FILENO].offset = 0;
    descriptors[STDOUT_FILENO].flags = 0;
    descriptors[STDOUT_FILENO].type = FileDescriptorType::File;
    descriptors[STDOUT_FILENO].pipeId = 0;

    descriptors[STDERR_FILENO].type = FileDescriptorType::File;
    descriptors[STDERR_FILENO].pipeId = 0;
    descriptors[STDERR_FILENO].used = true;
    descriptors[STDERR_FILENO].node = 0;
    descriptors[STDERR_FILENO].offset = 0;
    descriptors[STDERR_FILENO].flags = 0;

    return true;
}

int file_descriptor_allocate(
    uint64_t node,
    uint32_t flags
)
{
    for (uint32_t i = FIRST_USER_FILE_DESCRIPTOR;
         i < MAX_FILE_DESCRIPTORS;
         ++i)
    {
        if (!descriptors[i].used)
        {
            descriptors[i].used = true;
            descriptors[i].type = FileDescriptorType::File;
            descriptors[i].node = node;
            descriptors[i].offset = 0;
            descriptors[i].flags = flags;
            descriptors[i].pipeId = 0;

            return static_cast<int>(i);
        }
    }

    return -1;
}

int file_descriptor_allocate_directory(
    uint64_t node,
    uint32_t flags
)
{
    for (uint32_t i = FIRST_USER_FILE_DESCRIPTOR;
         i < MAX_FILE_DESCRIPTORS;
         ++i)
    {
        if (!descriptors[i].used)
        {
            descriptors[i].used = true;
            descriptors[i].type = FileDescriptorType::Directory;
            descriptors[i].node = node;
            descriptors[i].offset = 0;
            descriptors[i].flags = flags;
            descriptors[i].pipeId = 0;

            return static_cast<int>(i);
        }
    }

    return -1;
}

int file_descriptor_allocate_pipe(
    uint32_t pipeId,
    uint32_t flags
)
{
    for (uint32_t i = FIRST_USER_FILE_DESCRIPTOR;
         i < MAX_FILE_DESCRIPTORS;
         ++i)
    {
        if (!descriptors[i].used)
        {
            descriptors[i].used = true;
            descriptors[i].type = FileDescriptorType::Pipe;
            descriptors[i].node = 0;
            descriptors[i].offset = 0;
            descriptors[i].flags = flags;
            descriptors[i].pipeId = pipeId;

            return static_cast<int>(i);
        }
    }

    return -1;
}

bool file_descriptor_close(int fd)
{
    if (fd < FIRST_USER_FILE_DESCRIPTOR ||
        fd >= static_cast<int>(MAX_FILE_DESCRIPTORS))
    {
        return false;
    }

    if (!descriptors[fd].used)
    {
        return false;
    }

    descriptors[fd].used = false;
    descriptors[fd].type = FileDescriptorType::File;
    descriptors[fd].node = 0;
    descriptors[fd].offset = 0;
    descriptors[fd].flags = 0;
    descriptors[fd].pipeId = 0;

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
