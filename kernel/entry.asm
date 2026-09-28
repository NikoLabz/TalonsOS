bits 32

global _start
extern kernel_main

; Provided by linker.ld: the boundaries of the .bss section.
extern __bss_start
extern __bss_end

; Provided by linker.ld: the boundaries of the .init_array table of
; global C++ constructor function pointers.
extern __init_array_start
extern __init_array_end

; ============================================================
; TALONSOS x86-64 KERNEL ENTRY
; ============================================================
; The bootloader has already:
;
;   1. Loaded the kernel at 0x10000 (KERNEL_BASE in boot.asm,
;      '. = 0x10000' in linker.ld -- these MUST agree)
;   2. Enabled the A20 line
;   3. Enabled protected mode
;   4. Loaded a 32-bit GDT
;
; We now:
;
;   1. Zero the kernel's .bss (it is not stored in the image)
;   2. Build identity-mapped page tables
;   3. Enable PAE
;   4. Enable x86-64 long mode
;   5. Enable paging
;   6. Load a 64-bit GDT
;   7. Enter 64-bit mode
;   8. Call kernel_main()
;
; Memory used here (see the full map in linker.ld):
;
;   0x10000 ..  __bss_end   kernel image + .bss (limit 0x80000)
;   0x90000 - 0x92FFF       page tables
;   0x90000 and below       stack (grows down)
; ============================================================

_start:

    ; ========================================================
    ; TEST POINT
    ; ========================================================
    ; Write 'E' to VGA memory.
    ;
    ; If this appears, the kernel entry was reached.
    ; ========================================================

    mov word [0xB8000], 0x0745


    ; ========================================================
    ; ZERO .BSS
    ; ========================================================
    ; .bss is NOT part of the flat kernel image on disk, so the
    ; bootloader never loads or clears it, and RAM is not
    ; guaranteed to be zero (real hardware, warm reboots).
    ; C++ requires zero-initialised globals/statics to be zero,
    ; so clear [__bss_start, __bss_end) before any C++ code runs.
    ;
    ; linker.ld asserts that __bss_end <= 0x80000, so this can
    ; never reach the stack or the page tables at 0x90000, and it
    ; starts at 0x10000+, so it never touches the boot sector.
    ; This code does not use the stack or any .bss variable.

    cld
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb


    ; ========================================================
    ; SET UP PAGE TABLES
    ; ========================================================

    ; Clear the page-table area.
    ;
    ; We'll use:
    ;
    ;   0x90000 = PML4
    ;   0x91000 = PDPT
    ;   0x92000 = Page Directory
    ;
    ; This gives us an identity mapping for the first
    ; 2 MiB of physical memory.

    mov edi, 0x90000
    xor eax, eax
    mov ecx, 0x3000
    cld
    rep stosb


    ; ========================================================
    ; PML4
    ; ========================================================

    mov eax, 0x91003
    mov [0x90000], eax


    ; ========================================================
    ; PDPT
    ; ========================================================

    mov eax, 0x92003
    mov [0x91000], eax


    ; ========================================================
    ; PAGE DIRECTORY
    ; ========================================================
    ; Map the first 2 MiB using a 2 MiB huge page.
    ;
    ; Bit 0 = Present
    ; Bit 1 = Writable
    ; Bit 7 = Page Size
    ;
    ; 0x83 = 10000011b

    mov eax, 0x00000083
    mov [0x92000], eax


    ; ========================================================
    ; ENABLE PAE
    ; ========================================================

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax


    ; ========================================================
    ; LOAD PML4 INTO CR3
    ; ========================================================

    mov eax, 0x90000
    mov cr3, eax


    ; ========================================================
    ; ENABLE LONG MODE
    ; ========================================================

    mov ecx, 0xC0000080
    rdmsr

    or eax, 1 << 8

    wrmsr


    ; ========================================================
    ; ENABLE PAGING
    ; ========================================================

    mov eax, cr0

    or eax, 1 << 31

    mov cr0, eax


    ; ========================================================
    ; LOAD 64-BIT GDT
    ; ========================================================

    lgdt [gdt64_descriptor]


    ; ========================================================
    ; ENTER LONG MODE
    ; ========================================================

    jmp 0x08:long_mode


; ============================================================
; 64-BIT GDT
; ============================================================

gdt64_start:

gdt64_null:
    dq 0


; ------------------------------------------------------------
; 64-BIT CODE SEGMENT
; ------------------------------------------------------------

gdt64_code:
    dq 0x00AF9A000000FFFF


; ------------------------------------------------------------
; 64-BIT DATA SEGMENT
; ------------------------------------------------------------

gdt64_data:
    dq 0x00AF92000000FFFF


gdt64_end:


; ============================================================
; 64-BIT GDT DESCRIPTOR
; ============================================================

gdt64_descriptor:

    dw gdt64_end - gdt64_start - 1
    dq gdt64_start


; ============================================================
; LONG MODE
; ============================================================

bits 64

long_mode:

    ; ========================================================
    ; SET DATA SEGMENTS
    ; ========================================================

    mov ax, 0x10

    mov ds, ax
    mov es, ax
    mov ss, ax


    ; ========================================================
    ; SET UP 64-BIT STACK
    ; ========================================================

    mov rsp, 0x90000


    ; ========================================================
    ; TEST POINT
    ; ========================================================
    ; Write 'L' to VGA memory.
    ;
    ; If you see L, the CPU successfully reached 64-bit mode.
    ; ========================================================

    mov word [qword 0xB8002], 0x074C


    ; ========================================================
    ; RUN GLOBAL C++ CONSTRUCTORS
    ; ========================================================
    ; This kernel is linked directly with `ld` (no crt0/crti/
    ; crtbegin, no libc startup), so nothing else calls the
    ; constructors GCC records in .init_array for global/static
    ; objects that cannot be constant-initialized (a user-defined
    ; constructor, a reference member, ...).
    ;
    ; Without this, such an object is left exactly as the .bss-zero
    ; pass above wrote it: an all-zero vtable pointer and any
    ; reference members bound to nothing. The first virtual call
    ; on it loads a function pointer from the IVT at address 0 and
    ; triple-faults.
    ;
    ; Each entry in [__init_array_start, __init_array_end) is an
    ; 8-byte function pointer; call every one in order. rbx and r12
    ; are callee-saved under the SysV ABI, so the loop state
    ; survives each constructor.

    mov rbx, __init_array_start
    mov r12, __init_array_end

.run_ctors:

    cmp rbx, r12
    jae .ctors_done

    call [rbx]

    add rbx, 8
    jmp .run_ctors

.ctors_done:


    ; ========================================================
    ; CALL C++ KERNEL
    ; ========================================================

    call kernel_main


.hang:

    cli
    hlt
    jmp .hang
