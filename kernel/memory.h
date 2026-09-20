#pragma once

#include <cstddef>

void memory_init();

void* kmalloc(std::size_t size);
void kfree(void* ptr);
