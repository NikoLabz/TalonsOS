#include "pipe_manager.h"
#include "pipe.h"
#include "filesystem.h"
#include "file_descriptor.h"

#include <cstdint>

int main()
{
    if (!filesystem_init())
        return 1;

    int readFd = -1;
    int writeFd = -1;

    if (!filesystem_create_pipe(
            readFd,
            writeFd))
    {
        return 2;
    }

    if (readFd < FIRST_USER_FILE_DESCRIPTOR)
        return 3;

    if (writeFd < FIRST_USER_FILE_DESCRIPTOR)
        return 4;

    if (readFd == writeFd)
        return 5;

    FileDescriptor* readDescriptor =
        file_descriptor_get(readFd);

    FileDescriptor* writeDescriptor =
        file_descriptor_get(writeFd);

    if (!readDescriptor)
        return 6;

    if (!writeDescriptor)
        return 7;

    if (readDescriptor->type !=
        FileDescriptorType::Pipe)
        return 8;

    if (writeDescriptor->type !=
        FileDescriptorType::Pipe)
        return 9;

    if (readDescriptor->pipeId !=
        writeDescriptor->pipeId)
        return 10;

    if ((readDescriptor->flags &
         FILESYSTEM_OPEN_READ) == 0)
        return 11;

    if ((writeDescriptor->flags &
         FILESYSTEM_OPEN_WRITE) == 0)
        return 12;

    if ((readDescriptor->flags &
         FILESYSTEM_OPEN_WRITE) != 0)
        return 13;

    if ((writeDescriptor->flags &
     FILESYSTEM_OPEN_READ) != 0)
    return 14;

const uint8_t message[] =
    {'H', 'e', 'l', 'l', 'o'};

    if (!filesystem_write(
            writeFd,
            message,
            sizeof(message)))
        return 17;

       uint8_t output[sizeof(message)]{};

    int bytesRead =
        filesystem_read(
            readFd,
            output,
            sizeof(output)
        );

        uint8_t fullBuffer[
        PIPE_BUFFER_SIZE
    ]{};

    if (!filesystem_write(
            writeFd,
            fullBuffer,
            PIPE_BUFFER_SIZE))
        return 20;

    if (filesystem_write(
            writeFd,
            fullBuffer,
            1))
        return 21;

    if (bytesRead !=
        static_cast<int>(sizeof(message)))
        return 18;

    for (uint32_t i = 0;
         i < sizeof(message);
         ++i)
    {
        if (output[i] != message[i])
            return 19;
    }

        const uint32_t pipeId =
        readDescriptor->pipeId;


    if (!filesystem_close(readFd))
        return 15;

    // Closing the read end must not destroy the pipe
    // while the write end is still open.
    if (pipe_manager_get(
            static_cast<int>(pipeId)) == nullptr)
        return 22;

    if (!filesystem_close(writeFd))
        return 16;

    // Both endpoints are now closed, so the pipe
    // should have been destroyed.
        if (pipe_manager_get(
            static_cast<int>(pipeId)) != nullptr)
        return 23;

    int secondReadFd = -1;
    int secondWriteFd = -1;

    if (!filesystem_create_pipe(
            secondReadFd,
            secondWriteFd))
        return 24;

    FileDescriptor* secondReadDescriptor =
        file_descriptor_get(secondReadFd);

    if (!secondReadDescriptor)
        return 25;

    if (secondReadDescriptor->pipeId != pipeId)
        return 26;

    if (!filesystem_close(secondReadFd))
        return 27;

    if (!filesystem_close(secondWriteFd))
        return 28;

    return 0;
}
