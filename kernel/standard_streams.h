#pragma once

#include <cstdint>

bool standard_streams_init(
    unsigned short initialPosition
);

int stream_read(
    int fd,
    uint8_t* buffer,
    uint32_t size
);

bool stream_write(
    int fd,
    const uint8_t* data,
    uint32_t size
);

bool stream_write_char(
    int fd,
    char character
);

bool stream_write_string(
    int fd,
    const char* string
);

int stdin_read_char();

void stdout_write_char(
    char character
);

void stdout_write_string(
    const char* string
);

void stderr_write_char(
    char character
);

void stderr_write_string(
    const char* string
);
