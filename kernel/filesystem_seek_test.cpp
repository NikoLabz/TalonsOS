#include "filesystem.h"

int main()
{
    if (!filesystem_init())
        return 1;

    if (!filesystem_create_file("seek.txt"))
        return 2;

    int fd =
        filesystem_open(
            "seek.txt",
            FILESYSTEM_OPEN_READ |
            FILESYSTEM_OPEN_WRITE
        );

    if (fd < 0)
        return 3;

    const uint8_t data[] = "Hello TalonsOS";

    if (!filesystem_write(
            fd,
            data,
            sizeof(data) - 1))
    {
        return 4;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (!descriptor)
        return 5;

    if (descriptor->offset !=
        sizeof(data) - 1)
    {
        return 6;
    }

    if (!filesystem_seek(fd, 0))
        return 7;

    descriptor =
        file_descriptor_get(fd);

    if (!descriptor)
        return 8;

    if (descriptor->offset != 0)
        return 9;

    uint8_t buffer[32] = {};

    const int bytesRead =
        filesystem_read(
            fd,
            buffer,
            sizeof(buffer)
        );

    if (bytesRead !=
        static_cast<int>(
            sizeof(data) - 1))
    {
        return 10;
    }

    for (unsigned int i = 0;
         i < sizeof(data) - 1;
         ++i)
    {
        if (buffer[i] != data[i])
            return 11;
    }

    if (filesystem_seek(
            fd,
            sizeof(data) - 1))
    {
        descriptor =
            file_descriptor_get(fd);

        if (!descriptor)
            return 12;

        if (descriptor->offset !=
            sizeof(data) - 1)
        {
            return 13;
        }
    }
    else
    {
        return 14;
    }

    if (filesystem_seek(
            fd,
            sizeof(data)))
    {
        return 15;
    }

    if (!filesystem_close(fd))
        return 16;

    return 0;
}
