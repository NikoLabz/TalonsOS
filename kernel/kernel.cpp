#include "frame_allocator.h"
#include "e820.h"
#include "standard_streams.h"
#include "memory.h"
#include "filesystem.h"
#include "vga.h"
#include "terminal.h"
#include "block_device.h"

extern "C" char _kernel_end;

extern "C" void kernel_main()
{
    memory_init();

    const bool e820Valid = e820_init();
    bool framesReady = false;

    if (e820Valid)
    {
        const auto* header =
            reinterpret_cast<const talons_e820::Header*>(0x5FF0);
        const auto* entries =
            reinterpret_cast<const talons_e820::Entry*>(0x6000);

        const talons_frames::ReservedRange reserved[] = {
            {0, 0x10000},
            {0x10000, reinterpret_cast<uintptr_t>(&_kernel_end) - 0x10000},
            {0x80000, 0x13000},
            {0x100000, 0x100000}
        };

        framesReady = talons_frames::init(
            *header, entries, reserved,
            sizeof(reserved) / sizeof(reserved[0]));
    }

    const unsigned char colour = 0x0A;

    const char* message = "TALONSOS KERNEL ONLINE.";

    for (int i = 0; message[i] != '\0'; ++i)
        vga_putchar_at(i, message[i], colour);

    const char* e820Status = e820Valid
        ? "E820: VALID"
        : "E820: INVALID";

    for (int i = 0; e820Status[i] != '\0'; ++i)
        vga_putchar_at(125 + i, e820Status[i], colour);

    const bool fsInitOk = filesystem_init();

    const char* framesStatus = framesReady
        ? "Frames: ONLINE"
        : "Frames: OFFLINE";

    for (int i = 0; framesStatus[i] != '\0'; ++i)
        vga_putchar_at(145 + i, framesStatus[i], colour);

    const bool taloncOnline =
        fsInitOk && filesystem_is_mounted();

    const char* taloncStatus = taloncOnline
        ? "TalonC Compiler: ONLINE"
        : "TalonC Compiler: OFFLINE";

    if (!fsInitOk)
        return;

    const char* fsStatus = filesystem_is_mounted()
        ? "TalonsFS: mounted"
        : "TalonsFS: mount failed";

    for (int j = 0; fsStatus[j] != '\0'; ++j)
        vga_putchar_at(80 + j, fsStatus[j], colour);

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
