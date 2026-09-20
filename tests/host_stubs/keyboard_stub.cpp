// Host-only test double for kernel/keyboard.cpp.
//
// kernel/terminal.cpp's terminal_run() references keyboard_poll_char(),
// so the symbol must exist at link time even though these regression
// tests never call terminal_run() (they drive shell_handle_line()
// directly). Never linked into the real kernel or QEMU build.

#include "keyboard.h"

char keyboard_poll_char()
{
    return 0;
}
