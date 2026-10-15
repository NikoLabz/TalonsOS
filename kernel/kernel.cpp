#include "standard_streams.h"
#include "memory.h"
#include "filesystem.h"
#include "vga.h"
#include "terminal.h"
#include "block_device.h"

extern "C" void kernel_main()
{
    memory_init();

    const unsigned char colour = 0x07;

    // ========================================================
    // KERNEL MESSAGE
    // ========================================================

    const char* message = "TALONSOS KERNEL ONLINE.";

    for (int i = 0; message[i] != '\0'; ++i)
    {
        vga_putchar_at(i, message[i], colour);
    }

    const bool fsInitOk = filesystem_init();
    const bool taloncOnline = fsInitOk && filesystem_is_mounted();

    const char* taloncStatus = taloncOnline
        ? "TalonC Compiler: ONLINE"
        : "TalonC Compiler: OFFLINE";

    if (!fsInitOk)
    {
        return;
    }

    const char* fsStatus =
        filesystem_is_mounted()
            ? "TalonsFS: mounted"
            : "TalonsFS: mount failed";

    for (int j = 0; fsStatus[j] != '\0'; ++j)
    {
        vga_putchar_at(80 + j, fsStatus[j], colour);
    }

    int statusColumn = 80;
    for (int k = 0; fsStatus[k] != '\0'; ++k)
        ++statusColumn;

    statusColumn += 2;
    for (int k = 0; taloncStatus[k] != '\0'; ++k)
        vga_putchar_at(statusColumn + k, taloncStatus[k], colour);

    standard_streams_init(23);
 
    terminal_init();
    terminal_run();
}
