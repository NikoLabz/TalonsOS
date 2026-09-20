#include "../include/talonsfs/bitmap.h"

namespace TalonsFS {

Bitmap::Bitmap(
    uint8_t* data,
    uint64_t bitCount
)
    : data_(data),
      bitCount_(bitCount)
{
}

bool Bitmap::get(uint64_t index) const
{
    if (index >= bitCount_) {
        return false;
    }

    return (data_[index / 8] &
            (1u << (index % 8))) != 0;
}

void Bitmap::set(uint64_t index)
{
    if (index >= bitCount_) {
        return;
    }

    data_[index / 8] |=
        static_cast<uint8_t>(1u << (index % 8));
}

void Bitmap::clear(uint64_t index)
{
    if (index >= bitCount_) {
        return;
    }

    data_[index / 8] &=
        static_cast<uint8_t>(~(1u << (index % 8)));
}

uint64_t Bitmap::size() const
{
    return bitCount_;
}

}
