#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

namespace kfs
{
    constexpr unsigned int MAX_LIST_ENTRIES = 64;
    constexpr unsigned int MAX_NAME_LEN = 64;
    constexpr unsigned int MAX_FILE_READ_BYTES = 65536;

    struct ListEntry
    {
        char name[MAX_NAME_LEN];
        bool isDirectory;
    };
}

enum class FilesystemError
{
    None,
    NotMounted,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    NotAFile,
    NotADirectory,
    DirectoryNotEmpty,
    NoSpace,
    InvalidFilesystem,
    CorruptFilesystem,
    DeviceError,
    MetadataError,
    RecoveryError,
    AlreadyUnmounted,
    Unknown
};

bool filesystem_init();

bool filesystem_unmount();

bool filesystem_is_mounted();

FilesystemError filesystem_get_last_error();

/*
 * Human-readable name for a FilesystemError. Always returns a valid
 * static string, never nullptr, so callers can print it directly.
 */
const char* filesystem_error_string(FilesystemError error);

bool filesystem_format();

unsigned int filesystem_list_directory(
    kfs::ListEntry* out
);

bool filesystem_create_directory(
    const char* name
);

bool filesystem_create_file(
    const char* name
);

constexpr uint32_t FILESYSTEM_OPEN_READ = 1u << 0;
constexpr uint32_t FILESYSTEM_OPEN_WRITE = 1u << 1;

int filesystem_open(
    const char* name,
    uint32_t flags
);

bool filesystem_close(
    int fd
);

int filesystem_read(
    int fd,
    uint8_t* out,
    uint32_t outCapacity
);

bool filesystem_seek(
    int fd,
    uint64_t offset
);

bool filesystem_get_file_size(
    int fd,
    uint64_t* size
);

bool filesystem_write(
    int fd,
    const uint8_t* data,
    uint32_t size
);

bool filesystem_read_file(
    const char* name,
    uint8_t* out,
    uint32_t outCapacity,
    uint64_t* outSize
);

bool filesystem_write_file(
    const char* name,
    const uint8_t* data,
    uint32_t size
);

bool filesystem_remove(
    const char* name
);

bool filesystem_copy(
    const char* source,
    const char* destination
);

bool filesystem_move(
    const char* source,
    const char* destination
);

bool filesystem_change_directory(
    const char* name
);

const char* filesystem_current_directory_path();

#endif
