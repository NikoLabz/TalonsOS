#include "pipe_manager.h"

namespace
{
    Pipe pipes[MAX_PIPES];
    bool pipeUsed[MAX_PIPES];
}

bool pipe_manager_init()
{
    for (uint32_t i = 0; i < MAX_PIPES; ++i)
    {
        pipeUsed[i] = false;
        pipe_init(pipes[i]);
    }

    return true;
}

int pipe_manager_create()
{
    for (uint32_t i = 0; i < MAX_PIPES; ++i)
    {
        if (!pipeUsed[i])
        {
            pipeUsed[i] = true;
            pipe_init(pipes[i]);

            return static_cast<int>(i);
        }
    }

    return -1;
}

Pipe* pipe_manager_get(int pipeId)
{
    if (pipeId < 0 ||
        pipeId >= static_cast<int>(MAX_PIPES))
    {
        return nullptr;
    }

    if (!pipeUsed[pipeId])
        return nullptr;

    return &pipes[pipeId];
}

bool pipe_manager_destroy(int pipeId)
{
    if (pipeId < 0 ||
        pipeId >= static_cast<int>(MAX_PIPES))
    {
        return false;
    }

    if (!pipeUsed[pipeId])
        return false;

    pipeUsed[pipeId] = false;
    pipe_init(pipes[pipeId]);

    return true;
}
