#include "file_descriptor.h"

int main()
{
    if (!file_descriptors_init())
        return 1;

    // Standard descriptors are reserved and initialized.
    if (file_descriptor_get(STDIN_FILENO) == nullptr)
        return 2;

    if (file_descriptor_get(STDOUT_FILENO) == nullptr)
        return 3;

    if (file_descriptor_get(STDERR_FILENO) == nullptr)
        return 4;

    // Standard descriptors cannot be closed.
    if (file_descriptor_close(STDIN_FILENO))
        return 5;

    if (file_descriptor_close(STDOUT_FILENO))
        return 6;

    if (file_descriptor_close(STDERR_FILENO))
        return 7;

    // First descriptor should be 3.
    int fd1 = file_descriptor_allocate(42, 1);

    if (fd1 != FIRST_USER_FILE_DESCRIPTOR)
        return 8;

    FileDescriptor* descriptor = file_descriptor_get(fd1);

    if (!descriptor)
        return 9;

    if (descriptor->node != 42)
        return 10;

    if (descriptor->offset != 0)
        return 11;

    if (descriptor->flags != 1)
        return 12;

    // Closing should invalidate the descriptor.
    if (!file_descriptor_close(fd1))
        return 13;

    if (file_descriptor_get(fd1) != nullptr)
        return 14;

    // Closing an already-closed descriptor must fail.
    if (file_descriptor_close(fd1))
        return 15;

    // The descriptor number should be reusable.
    int fd2 = file_descriptor_allocate(99, 7);

    if (fd2 != 3)
        return 16;

    descriptor = file_descriptor_get(fd2);

    if (!descriptor)
        return 17;

    if (descriptor->node != 99)
        return 18;

    if (descriptor->flags != 7)
        return 19;

    // Invalid descriptors must be rejected.
    if (file_descriptor_get(-1) != nullptr)
        return 20;

    if (file_descriptor_get(MAX_FILE_DESCRIPTORS) != nullptr)
        return 21;

    if (file_descriptor_close(-1))
        return 22;

    if (file_descriptor_close(MAX_FILE_DESCRIPTORS))
        return 23;

    // Fill every available descriptor.
    int descriptors[MAX_FILE_DESCRIPTORS - 3];

    descriptors[0] = fd2;

    for (uint32_t i = 1; i < MAX_FILE_DESCRIPTORS - 3; ++i)
    {
        descriptors[i] =
            file_descriptor_allocate(100 + i, 0);

        if (descriptors[i] < 3)
            return 24;
    }

    // There should now be no free descriptors.
    if (file_descriptor_allocate(999, 0) != -1)
        return 25;

    // Free one descriptor and make sure it becomes available again.
    if (!file_descriptor_close(descriptors[5]))
        return 26;

    int reused = file_descriptor_allocate(500, 3);

    if (reused != descriptors[5])
        return 27;

    FileDescriptor* reused_descriptor =
        file_descriptor_get(reused);

    if (!reused_descriptor)
        return 28;

    if (reused_descriptor->node != 500)
        return 29;

    if (reused_descriptor->offset != 0)
        return 30;

    if (reused_descriptor->flags != 3)
        return 31;

        // Pipe descriptors should be marked as pipes.
    if (!file_descriptor_close(reused))
        return 32;

    int pipe_fd = file_descriptor_allocate_pipe(7, 11);

    if (pipe_fd != reused)
        return 33;

    FileDescriptor* pipe_descriptor =
        file_descriptor_get(pipe_fd);

    if (!pipe_descriptor)
        return 34;

    if (pipe_descriptor->type != FileDescriptorType::Pipe)
        return 35;

    if (pipe_descriptor->pipeId != 7)
        return 36;

    if (pipe_descriptor->node != 0)
        return 37;

    if (pipe_descriptor->offset != 0)
        return 38;

    if (pipe_descriptor->flags != 11)
        return 39;

        // Reusing a pipe descriptor as a normal file must clear pipe metadata.
    if (!file_descriptor_close(pipe_fd))
        return 40;

    int reused_file =
        file_descriptor_allocate(123, 55);

        if (reused_file != pipe_fd)
        return 41;

    FileDescriptor* reused_file_descriptor =
        file_descriptor_get(reused_file);

    if (!reused_file_descriptor)
        return 42;

    if (reused_file_descriptor->type != FileDescriptorType::File)
        return 43;

    if (reused_file_descriptor->pipeId != 0)
        return 44;

    if (reused_file_descriptor->node != 123)
        return 45;

    if (reused_file_descriptor->offset != 0)
        return 46;

    if (reused_file_descriptor->flags != 55)
        return 47;

    return 0;

}
