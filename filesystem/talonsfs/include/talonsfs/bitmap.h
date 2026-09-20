#pragma once

#include <cstdint>

namespace TalonsFS {

    class Bitmap {
    public:
        Bitmap(uint8_t* data, uint64_t bitCount);

        bool get(uint64_t index) const;
        void set(uint64_t index);
        void clear(uint64_t index);
        uint64_t size() const;

    private:
        uint8_t* data_;
        uint64_t bitCount_;
    };

}
