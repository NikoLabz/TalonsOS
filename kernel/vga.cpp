#include "vga.h"

namespace
{
    volatile unsigned short* const video =
        (volatile unsigned short*)0xB8000;
}

void vga_putchar_at(int position, char c, unsigned char colour)
{
    video[position] =
        (colour << 8) | (unsigned char)c;
}


// ============================================================
// SCROLL SCREEN UP ONE ROW
// ============================================================

void vga_scroll(unsigned char colour)
{
    // Move rows 1-24 upward into rows 0-23.

    for (int row = 1; row < 25; ++row)
    {
        for (int column = 0; column < 80; ++column)
        {
            int source =
                row * 80 + column;

            int destination =
                (row - 1) * 80 + column;

            video[destination] =
                video[source];
        }
    }


    // Clear the final row.

    for (int column = 0; column < 80; ++column)
    {
        int position =
            24 * 80 + column;

        video[position] =
            (colour << 8) | ' ';
    }
}


// ============================================================
// HARDWARE CURSOR
// ============================================================

void vga_update_cursor(unsigned short position)
{
    unsigned short port;
    unsigned char value;

    // Select cursor low-byte register.
    port = 0x3D4;
    value = 0x0F;

    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );

    // Write cursor low byte.
    port = 0x3D5;
    value = position & 0xFF;

    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );

    // Select cursor high-byte register.
    port = 0x3D4;
    value = 0x0E;

    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );

    // Write cursor high byte.
    port = 0x3D5;
    value = (position >> 8) & 0xFF;

    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// Save and restore all 80x25 VGA text cells.
void vga_save_screen(unsigned short* buffer)
{
    if (buffer == nullptr)
        return;

    for (int i = 0; i < 2000; ++i)
        buffer[i] = video[i];
}

void vga_restore_screen(const unsigned short* buffer)
{
    if (buffer == nullptr)
        return;

    for (int i = 0; i < 2000; ++i)
        video[i] = buffer[i];
}
