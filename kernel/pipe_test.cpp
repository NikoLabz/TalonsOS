#include "pipe.h"

#include <cstring>
#include <iostream>

int main()
{
    Pipe pipe{};

    pipe_init(pipe);

    const char message[] = "Hello, TalonsOS!";
    uint8_t output[sizeof(message)] = {};

    if (!pipe_write(
            pipe,
            reinterpret_cast<const uint8_t*>(message),
            sizeof(message)))
    {
        std::cout << "Pipe write failed\n";
        return 1;
    }

    if (!pipe_read(
            pipe,
            output,
            sizeof(message)))
    {
        std::cout << "Pipe read failed\n";
        return 1;
    }

    if (std::memcmp(
            output,
            message,
            sizeof(message)) != 0)
    {
        std::cout << "Pipe data corrupted\n";
        return 1;
    }

    if (pipe.size != 0)
    {
        std::cout << "Pipe was not emptied\n";
        return 1;
    }

    std::cout << "Pipe basic test: PASSED\n";

    return 0;
}
