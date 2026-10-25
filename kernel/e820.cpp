#include "e820.h"

namespace
{
    constexpr uintptr_t E820_HEADER_ADDRESS = 0x5FF0;
    constexpr uintptr_t E820_ENTRIES_ADDRESS = 0x6000;

    bool map_valid = false;
}

bool e820_init()
{
    const auto* header =
        reinterpret_cast<const talons_e820::Header*>(
            E820_HEADER_ADDRESS);

    const auto* entries =
        reinterpret_cast<const talons_e820::Entry*>(
            E820_ENTRIES_ADDRESS);

    map_valid = talons_e820::validMap(*header, entries);
    return map_valid;
}

bool e820_is_valid()
{
    return map_valid;
}
