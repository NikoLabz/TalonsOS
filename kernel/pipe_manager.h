#pragma once

#include "pipe.h"

constexpr uint32_t MAX_PIPES = 16;

bool pipe_manager_init();

int pipe_manager_create();

Pipe* pipe_manager_get(int pipeId);

bool pipe_manager_destroy(int pipeId);
