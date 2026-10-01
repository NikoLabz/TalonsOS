#include "pipe_manager.h"

#include <iostream>

int main()
{
    if (!pipe_manager_init())
    {
        std::cout << "Pipe manager init failed\n";
        return 1;
    }

    int first = pipe_manager_create();

    if (first < 0)
    {
        std::cout << "First pipe creation failed\n";
        return 1;
    }

    Pipe* pipe = pipe_manager_get(first);

    if (pipe == nullptr)
    {
        std::cout << "Created pipe could not be retrieved\n";
        return 1;
    }

    const uint8_t message[] = "TalonsOS";

    if (!pipe_write(
            *pipe,
            message,
            sizeof(message)))
    {
        std::cout << "Pipe write failed\n";
        return 1;
    }

    uint8_t output[sizeof(message)] = {};

    if (!pipe_read(
            *pipe,
            output,
            sizeof(output)))
    {
        std::cout << "Pipe read failed\n";
        return 1;
    }

    for (uint32_t i = 0; i < sizeof(message); ++i)
    {
        if (output[i] != message[i])
        {
            std::cout << "Pipe data corrupted\n";
            return 1;
        }
    }

    if (!pipe_manager_destroy(first))
    {
        std::cout << "Pipe destruction failed\n";
        return 1;
    }

    if (pipe_manager_get(first) != nullptr)
    {
        std::cout << "Destroyed pipe is still accessible\n";
        return 1;
    }

    std::cout << "Pipe manager test: PASSED\n";

    return 0;
}
