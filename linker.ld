/*
 * TalonsOS kernel linker script.
 *
 * ------------------------------------------------------------
 * PHYSICAL MEMORY MAP (this file, boot/boot.asm and
 * kernel/entry.asm MUST agree)
 * ------------------------------------------------------------
 *   0x00000 - 0x004FF   IVT + BIOS data area          (never touch)
 *   0x00500 - 0x07BFF   real-mode stack (grows down from 0x7C00)
 *   0x07C00 - 0x07DFF   boot sector + 32-bit GDT      (dead after handoff)
 *   0x07E00 - 0x0FFFF   spare
 *   0x10000 - 0x1FDFF   kernel flat image  (max 127 sectors = disk blocks 1-127)
 *   after image         kernel .bss  (NOT stored in the image; entry.asm
 *                       zeroes __bss_start .. __bss_end at boot)
 *   ...     - 0x7FFFF   limit for kernel image + .bss (checked below)
 *   0x80000 - 0x8FFFF   kernel stack headroom
 *   0x90000             initial stack top (grows down)
 *   0x90000 - 0x92FFF   page tables (built by entry.asm)
 *   0x9FC00 - 0x9FFFF   EBDA (BIOS)                   (never touch)
 *   0xA0000 - 0xFFFFF   video memory / ROMs           (never touch)
 *   0x100000 - 0x1FFFFF kernel heap (kernel/memory.cpp; needs A20,
 *                       which boot.asm enables)
 *
 * boot.asm loads the flat image to 0x10000 and does `call eax`
 * with eax = 0x10000, so _start (in entry.asm) must land exactly
 * there. The address below MUST equal KERNEL_BASE in boot.asm:
 * every absolute reference in the kernel (lgdt [gdt64_descriptor],
 * the far jump into long mode, vtables, string pointers...) is
 * resolved against this address at link time.
 *
 * SIZE LIMIT: boot.asm reads the whole flat image
 * (kernel_flat.bin) with one INT 13h LBA read, and TalonsFS begins
 * at disk block 128, so the image must be <= 127 sectors
 * (65024 bytes). boot.asm fails the build if the image is bigger.
 */

ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    /*
     * Global C++ constructors. GCC registers a function pointer here
     * for every translation unit that has a global/static object
     * which cannot be fully constant-initialized (e.g. one with a
     * user-defined constructor, a reference member, or dynamic
     * state that isn't known at compile time). This kernel is linked
     * directly with `ld` -- no crt0/crti/crtbegin -- so nothing runs
     * these automatically; entry.asm walks
     * [__init_array_start, __init_array_end) itself before calling
     * kernel_main. KEEP prevents an unused-section GC pass from ever
     * discarding these entries.
     */
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    /*
     * .bss is not stored in kernel_flat.bin, so nothing loads or
     * clears it. entry.asm zeroes [__bss_start, __bss_end) before
     * any C++ code runs.
     */
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    _kernel_end = .;

    /*
     * .eh_frame is unwind data emitted by GCC even with
     * -fno-exceptions. Nothing in this kernel ever unwinds, but as an
     * orphan section it was being placed in the loadable image and
     * counted against the size limit.
     */
    /DISCARD/ : {
        *(.eh_frame*)
    }
}

/*
 * .bss occupies RAM at run time even though it is not in the image.
 * It must stay clear of the stack and page tables at 0x90000. Fail
 * the build instead of corrupting memory silently.
 */
ASSERT(_kernel_end <= 0x80000,
       "kernel image + .bss runs past 0x80000 and would collide with the stack/page tables at 0x90000")
