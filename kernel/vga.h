#ifndef VGA_H
#define VGA_H

// ============================================================
// VGA TEXT-MODE OUTPUT AND HARDWARE CURSOR
// ============================================================

void vga_putchar_at(int position, char c, unsigned char colour);

void vga_update_cursor(unsigned short position);

// Scrolls the VGA text screen up by one row.
void vga_scroll(unsigned char colour);

#endif
