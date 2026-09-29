#include "file_descriptor.h"

int main()
{
    if (!file_descriptors_init())
        return 1;

    // Standard descriptors are reserved.
    if (file_descriptor_get(0) != nullptr)
        return 2;

    if (file_descriptor_get(1) != nullptr)
        return 3;

    if (file_descriptor_get(2) != nullptr)
        return 4;

    // First descriptor should be 3.
    int fd1 = file_descriptor_allocate(42, 1);

    if (fd1 != 3)
        return 5;

    FileDescriptor* descriptor = file_descriptor_get(fd1);

    if (!descriptor)
        return 6;

    if (descriptor->node != 42)
        return 7;

    if (descriptor->offset != 0)
        return 8;

    if (descriptor->flags != 1)
        return 9;

    // Closing should invalidate the descriptor.
    if (!file_descriptor_close(fd1))
        return 10;

    if (file_descriptor_get(fd1) != nullptr)
        return 11;

    // Closing an already-closed descriptor must fail.
    if (file_descriptor_close(fd1))
        return 12;

    // The descriptor number should be reusable.
    int fd2 = file_descriptor_allocate(99, 7);

    if (fd2 != 3)
        return 13;

    descriptor = file_descriptor_get(fd2);

    if (!descriptor)
        return 14;

    if (descriptor->node != 99)
        return 15;

    if (descriptor->flags != 7)
        return 16;

    // Invalid descriptors must be rejected.
    if (file_descriptor_get(-1) != nullptr)
        return 17;

    if (file_descriptor_get(MAX_FILE_DESCRIPTORS) != nullptr)
        return 18;

    if (file_descriptor_close(-1))
        return 19;

    if (file_descriptor_close(MAX_FILE_DESCRIPTORS))
        return 20;

    // Fill every available descriptor.
    int descriptors[MAX_FILE_DESCRIPTORS - 3];

    descriptors[0] = fd2;

    for (uint32_t i = 1; i < MAX_FILE_DESCRIPTORS - 3; ++i)
    {
        descriptors[i] =
            file_descriptor_allocate(100 + i, 0);

        if (descriptors[i] < 3)
            return 21;
    }

    // There should now be no free descriptors.
    if (file_descriptor_allocate(999, 0) != -1)
        return 22;

    // Free one descriptor and make sure it becomes available again.
    if (!file_descriptor_close(descriptors[5]))
        return 23;

    int reused = file_descriptor_allocate(500, 3);

    if (reused != descriptors[5])
        return 24;

    FileDescriptor* reused_descriptor =
        file_descriptor_get(reused);

    if (!reused_descriptor)
        return 25;

    if (reused_descriptor->node != 500)
        return 26;

    if (reused_descriptor->offset != 0)
        return 27;

    if (reused_descriptor->flags != 3)
        return 28;

    return 0;
}
