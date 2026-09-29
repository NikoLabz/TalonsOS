#pragma once

#include <cstdint>

uint32_t talons_file_crc32c(
    const uint8_t* data,
    uint64_t size
);
