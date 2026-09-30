#pragma once

#include <cstdint>

constexpr uint32_t MAX_FILE_DESCRIPTORS = 32;

constexpr int STDIN_FILENO  = 0;
constexpr int STDOUT_FILENO = 1;
constexpr int STDERR_FILENO = 2;

constexpr int FIRST_USER_FILE_DESCRIPTOR = 3;

enum class FileDescriptorType : uint8_t
{
    File,
    Pipe
};

struct FileDescriptor
{
    bool used;
    FileDescriptorType type;
    uint64_t node;
    uint64_t offset;
    uint32_t flags;
    uint32_t pipeId;
};

bool file_descriptors_init();

int file_descriptor_allocate(
    uint64_t node,
    uint32_t flags
);

int file_descriptor_allocate_pipe(
    uint32_t pipeId,
    uint32_t flags
);

bool file_descriptor_close(
    int fd
);

FileDescriptor* file_descriptor_get(
    int fd
);
