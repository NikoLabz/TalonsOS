#include <iostream>
#include <cstdint>

#include "../include/talonsfs/bitmap.h"

int main()
{
    uint8_t data[2] = {};

    TalonsFS::Bitmap bitmap(data, 16);

    std::cout << "Initial bit 0: "
              << bitmap.get(0)
              << '\n';

    bitmap.set(0);

    std::cout << "After set bit 0: "
              << bitmap.get(0)
              << '\n';

    bitmap.clear(0);

    std::cout << "After clear bit 0: "
              << bitmap.get(0)
              << '\n';

    bitmap.set(8);

    std::cout << "After set bit 8: "
              << bitmap.get(8)
              << '\n';

    std::cout << "Bit 0 after setting bit 8: "
              << bitmap.get(0)
              << '\n';

    bitmap.clear(8);

    std::cout << "After clear bit 8: "
              << bitmap.get(8)
              << '\n';

    return 0;
}
