// Host-only test double for kernel/vga.cpp.
//
// Implements the exact same interface declared in kernel/vga.h
// (UNMODIFIED), but backed by a plain in-memory buffer instead of
// the real memory-mapped VGA text buffer at 0xB8000 and real I/O
// ports for the hardware cursor -- so kernel/terminal.cpp and
// kernel/shell.cpp, completely unmodified, can be exercised on the
// host for regression testing.
//
// This file lives outside kernel/ and is never linked into the real
// kernel or QEMU build; it only replaces vga.cpp for host-side
// regression tests.

#include "vga.h"

#include <cstdint>

namespace {
    // Same size class as the real VGA aperture: enough to hold the
    // visible 80x25 (2000-cell) screen plus extra "off-screen" cells,
    // so an out-of-range write (the exact bug this test exists to
    // catch) lands somewhere observable instead of corrupting
    // unrelated memory.
    uint16_t videoBuffer[8192] = {};

    unsigned short lastCursorPosition = 0;
}

void vga_putchar_at(int position, char c, unsigned char colour)
{
    if (position < 0 ||
        position >= static_cast<int>(
            sizeof(videoBuffer) / sizeof(videoBuffer[0])))
    {
        return;
    }

    videoBuffer[position] =
        static_cast<uint16_t>((colour << 8) | (unsigned char)c);
}

void vga_scroll(unsigned char colour)
{
    for (int row = 1; row < 25; ++row)
    {
        for (int column = 0; column < 80; ++column)
        {
            videoBuffer[(row - 1) * 80 + column] =
                videoBuffer[row * 80 + column];
        }
    }

    for (int column = 0; column < 80; ++column)
    {
        videoBuffer[24 * 80 + column] =
            static_cast<uint16_t>((colour << 8) | ' ');
    }
}

void vga_update_cursor(unsigned short position)
{
    lastCursorPosition = position;
}

void vga_save_screen(unsigned short* destination)
{
    if (!destination)
        return;

    for (unsigned int i = 0;
         i < sizeof(videoBuffer) / sizeof(videoBuffer[0]); ++i)
    {
        destination[i] = videoBuffer[i];
    }
}

void vga_restore_screen(const unsigned short* source)
{
    if (!source)
        return;

    for (unsigned int i = 0;
         i < sizeof(videoBuffer) / sizeof(videoBuffer[0]); ++i)
    {
        videoBuffer[i] = source[i];
    }
}

// ---- test-only accessors, not part of the real vga.h interface ----

char test_vga_char_at(int cell)
{
    if (cell < 0 ||
        cell >= static_cast<int>(
            sizeof(videoBuffer) / sizeof(videoBuffer[0])))
    {
        return '\0';
    }

    return static_cast<char>(videoBuffer[cell] & 0xFF);
}

unsigned short test_vga_last_cursor()
{
    return lastCursorPosition;
}
