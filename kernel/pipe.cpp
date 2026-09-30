#include "pipe.h"

void pipe_init(Pipe& pipe)
{
    pipe.readPos = 0;
    pipe.writePos = 0;
    pipe.size = 0;

    pipe.readOpen = true;
    pipe.writeOpen = true;
}

bool pipe_write(
    Pipe& pipe,
    const uint8_t* data,
    uint32_t size
)
{
    if (data == nullptr || size == 0)
        return false;

    if (!pipe.writeOpen)
        return false;

    if (size > PIPE_BUFFER_SIZE - pipe.size)
        return false;

    for (uint32_t i = 0; i < size; ++i)
    {
        pipe.buffer[pipe.writePos] = data[i];

        pipe.writePos =
            (pipe.writePos + 1) % PIPE_BUFFER_SIZE;
    }

    pipe.size += size;

    return true;
}

bool pipe_read(
    Pipe& pipe,
    uint8_t* data,
    uint32_t size
)
{
    if (data == nullptr || size == 0)
        return false;

    if (!pipe.readOpen)
        return false;

    if (size > pipe.size)
        return false;

    for (uint32_t i = 0; i < size; ++i)
    {
        data[i] = pipe.buffer[pipe.readPos];

        pipe.readPos =
            (pipe.readPos + 1) % PIPE_BUFFER_SIZE;
    }

    pipe.size -= size;

    return true;
}

void pipe_close_read(Pipe& pipe)
{
    pipe.readOpen = false;
}

void pipe_close_write(Pipe& pipe)
{
    pipe.writeOpen = false;
}
