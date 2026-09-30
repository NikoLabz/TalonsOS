#pragma once

#include <cstdint>

constexpr uint32_t PIPE_BUFFER_SIZE = 4096;

struct Pipe
{
    uint8_t buffer[PIPE_BUFFER_SIZE];

    uint32_t readPos;
    uint32_t writePos;
    uint32_t size;

    bool readOpen;
    bool writeOpen;
};

void pipe_init(Pipe& pipe);

bool pipe_write(
    Pipe& pipe,
    const uint8_t* data,
    uint32_t size
);

bool pipe_read(
    Pipe& pipe,
    uint8_t* data,
    uint32_t size
);

void pipe_close_read(Pipe& pipe);

void pipe_close_write(Pipe& pipe);
