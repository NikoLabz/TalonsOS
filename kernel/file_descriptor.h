#pragma once

#include <cstdint>

constexpr uint32_t MAX_FILE_DESCRIPTORS = 32;

struct FileDescriptor
{
    bool used;
    uint64_t node;
    uint64_t offset;
    uint32_t flags;
};

bool file_descriptors_init();

int file_descriptor_allocate(
    uint64_t node,
    uint32_t flags
);

bool file_descriptor_close(
    int fd
);

FileDescriptor* file_descriptor_get(
    int fd
);
