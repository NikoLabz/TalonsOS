#include "standard_streams.h"

#include "file_descriptor.h"
#include "filesystem.h"
#include "keyboard.h"
#include "pipe_manager.h"
#include "terminal.h"
#include "vga.h"

namespace
{
    constexpr unsigned char STDOUT_COLOUR = 0x07;

    void write_terminal_char(
    int fd,
    char character
)
{
    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        return;
    }

    unsigned short position =
        terminal_get_output_position();

    if (character == '\n')
    {
        position =
            static_cast<unsigned short>(
                (position / 80 + 1) * 80
            );

        terminal_set_output_position(position);
        return;
    }

    vga_putchar_at(
        position,
        character,
        STDOUT_COLOUR
    );

    ++position;

    if (position >= 2000)
    {
        vga_scroll(STDOUT_COLOUR);
        position -= 80;
    }

    terminal_set_output_position(position);
}

    bool write_terminal_string(
        int fd,
        const uint8_t* data,
        uint32_t size
    )
    {
        if (data == nullptr)
        {
            return false;
        }

        for (uint32_t i = 0;
             i < size;
             ++i)
        {
            write_terminal_char(
                fd,
                static_cast<char>(data[i])
            );
        }

        return true;
    }
}

bool standard_streams_init(
    unsigned short initialPosition
)
{
    FileDescriptor* stdoutDescriptor =
        file_descriptor_get(STDOUT_FILENO);

    if (stdoutDescriptor == nullptr)
    {
        return false;
    }

    stdoutDescriptor->offset =
        initialPosition;

    FileDescriptor* stderrDescriptor =
        file_descriptor_get(STDERR_FILENO);

    if (stderrDescriptor == nullptr)
    {
        return false;
    }

    stderrDescriptor->offset =
        initialPosition;

    return true;
}

int stream_read(
    int fd,
    uint8_t* buffer,
    uint32_t size
)
{
    if (buffer == nullptr || size == 0)
    {
        return -1;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        return -1;
    }

    /*
     * Pipes and normal filesystem files are
     * handled by the filesystem layer.
     */
    return filesystem_read(
        fd,
        buffer,
        size
    );
}

bool stream_write(
    int fd,
    const uint8_t* data,
    uint32_t size
)
{
    if (data == nullptr || size == 0)
    {
        return false;
    }

    FileDescriptor* descriptor =
        file_descriptor_get(fd);

    if (descriptor == nullptr)
    {
        return false;
    }

    /*
     * Standard output/error use the terminal.
     *
     * This is deliberately checked here instead
     * of inside filesystem_write(), because FD 1
     * and FD 2 represent the shell terminal.
     */
    if (fd == STDOUT_FILENO ||
        fd == STDERR_FILENO)
    {
        return write_terminal_string(
            fd,
            data,
            size
        );
    }

    /*
     * Everything else goes through the
     * filesystem abstraction.
     *
     * This includes normal files and pipes.
     */
    return filesystem_write(
        fd,
        data,
        size
    );
}

bool stream_write_char(
    int fd,
    char character
)
{
    const uint8_t byte =
        static_cast<uint8_t>(character);

    return stream_write(
        fd,
        &byte,
        1
    );
}

bool stream_write_string(
    int fd,
    const char* string
)
{
    if (string == nullptr)
    {
        return false;
    }

    uint32_t length = 0;

    while (string[length] != '\0')
    {
        ++length;
    }

    if (length == 0)
    {
        return true;
    }

    return stream_write(
        fd,
        reinterpret_cast<const uint8_t*>(
            string
        ),
        length
    );
}

int stdin_read_char()
{
    FileDescriptor* stdinDescriptor =
        file_descriptor_get(STDIN_FILENO);

    if (stdinDescriptor == nullptr)
    {
        return -1;
    }

    return static_cast<unsigned char>(
        keyboard_poll_char()
    );
}

void stdout_write_char(
    char character
)
{
    stream_write_char(
        STDOUT_FILENO,
        character
    );
}

void stdout_write_string(
    const char* string
)
{
    stream_write_string(
        STDOUT_FILENO,
        string
    );
}

void stderr_write_char(
    char character
)
{
    stream_write_char(
        STDERR_FILENO,
        character
    );
}

void stderr_write_string(
    const char* string
)
{
    stream_write_string(
        STDERR_FILENO,
        string
    );
}
