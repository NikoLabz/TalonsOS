; ============================================================
; TALONSOS BOOTLOADER
; ============================================================
; This is the first code executed by the computer.
;
; Responsibilities:
;   1. Set up the CPU
;   2. Load the kernel from disk (INT 13h extended / LBA read)
;   3. Enable the A20 line
;   4. Set up the GDT
;   5. Enter 32-bit protected mode
;   6. Jump to the kernel
;
; Stage two is loaded at physical address 0x8000 by stage one.
;
; ------------------------------------------------------------
; PHYSICAL MEMORY MAP  (linker.ld and kernel/entry.asm MUST agree)
; ------------------------------------------------------------
;   0x00000 - 0x004FF   IVT + BIOS data area
;   0x00500 - 0x04FFF   real-mode stack (grows down from 0x5000)
;   0x06000 - 0x065FF   BIOS E820 memory-map entries
;   0x10000 - 0x1F5FF   kernel flat image  (max 123 sectors)
;   0x90000             protected-mode stack top
;
; Disk layout:  block 0 = this boot sector
;               blocks 1-127 = kernel
;               block 128+ = TalonsFS
; ============================================================

bits 16
org 0x8000


; ============================================================
; CONFIGURATION
; ============================================================

; The kernel is loaded to real-mode segment 0x1000, offset 0,
; which is physical address 0x10000.
;
; KERNEL_BASE MUST equal the load address in linker.ld
; ('. = 0x10000;'). If they differ, every absolute address in the
; kernel is wrong and the CPU faults inside entry.asm.
KERNEL_SEGMENT      equ 0x1000
KERNEL_BASE         equ KERNEL_SEGMENT * 16

; The kernel starts at disk block 1 (block 0 is this boot sector).
KERNEL_LBA          equ 5

; E820 handoff: 16-byte header, followed by up to 64 24-byte entries.
E820_HEADER         equ 0x5FF0
E820_ENTRIES        equ 0x6000
E820_MAX_ENTRIES    equ 64
E820_ENTRY_SIZE     equ 24
E820_MAGIC          equ 0x304D454D


; The disk layout reserves blocks 1-127 for the kernel; TalonsFS
; starts at block 128. 127 sectors is also the most that many BIOSes
; accept in one INT 13h extended read, and 127 * 512 = 0xFE00 bytes
; fits inside one 64 KiB segment, so the transfer can never wrap
; past the end of segment 0x1000.
MAX_KERNEL_SECTORS  equ 123

; Number of kernel sectors is provided automatically
; by the Makefile.
%ifndef KERNEL_SECTORS
    %error "KERNEL_SECTORS was not provided"
%endif

%if KERNEL_SECTORS > MAX_KERNEL_SECTORS
    %error "Kernel image too large: it would run into TalonsFS at block 128"
%endif


; ============================================================
; BOOTLOADER ENTRY
; ============================================================

start:
    cli

    ; Set up the segment registers.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Give ourselves a stack. It grows DOWN from 0x7C00, into the
    ; free area 0x0500-0x7BFF, which nothing else uses.
    mov sp, 0x5000

    sti

    ; BIOS gives us the boot drive number in DL.
    mov [boot_drive], dl


; ============================================================
; INITIAL BOOT MESSAGE
; ============================================================

    mov si, loading_message
    call print_string

    ; Collect the BIOS physical-memory map before loading the kernel.
    call collect_e820
    jc e820_error

    mov si, e820_ok_message
    call print_string
    jmp load_kernel


; ============================================================
; BIOS E820 MEMORY MAP
; ============================================================
; Header at 0x5FF0: magic, count, entry size, reserved (4 dwords).
; Entries at 0x6000: base (qword), length (qword), type (dword),
; attributes (dword). The map remains below the stage-two image.
; Returns CF=1 on failure, CF=0 on success.
; ============================================================

collect_e820:
    pushad
    push es
    push di

    mov byte [e820_status], 1
    mov dword [E820_HEADER], 0
    mov dword [E820_HEADER + 4], 0
    mov dword [E820_HEADER + 8], E820_ENTRY_SIZE
    mov dword [E820_HEADER + 12], 0

    xor ebx, ebx

.next:
    cmp dword [E820_HEADER + 4], E820_MAX_ENTRIES
    jae .fail

    ; Destination = 0x6000 + count * 24.
    mov eax, dword [E820_HEADER + 4]
    imul eax, E820_ENTRY_SIZE
    add eax, E820_ENTRIES
    mov di, ax
    xor ax, ax
    mov es, ax

    ; Some BIOSes only return the 20-byte E820 record.
    mov dword [es:di + 20], 1

    ; EBX is the E820 continuation token.
    ; The BIOS must be allowed to return its updated value.
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, E820_ENTRY_SIZE
    int 0x15

    jc .fail
    cmp eax, 0x534D4150
    jne .fail
    cmp ecx, 20
    jb .fail

    inc dword [E820_HEADER + 4]
    test ebx, ebx
    jnz .next

    cmp dword [E820_HEADER + 4], 0
    je .fail

    mov dword [E820_HEADER], E820_MAGIC
    mov byte [e820_status], 0
    jmp .restore

.fail:
    mov dword [E820_HEADER], 0
    mov dword [E820_HEADER + 4], 0

.restore:
    pop di
    pop es
    popad

    cmp byte [e820_status], 0
    jne .return_fail
    clc
    ret

.return_fail:
    stc
    ret

e820_error:
    mov si, e820_error_message
    call print_string
    jmp hang16

; ============================================================
; LOAD KERNEL FROM DISK
; ============================================================
load_kernel:
; We use the BIOS INT 13h EXTENDED read (AH=42h), which addresses
; the disk by LBA. The old CHS read (AH=02h) needs the disk geometry
; and is not allowed to cross a track boundary (typically 63
; sectors, and we start at sector 2), which caps the kernel at
; about 62 sectors. LBA has no such limit.
; ============================================================

    ; Are INT 13h extensions supported?
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc no_extensions
    cmp bx, 0xAA55
    jne no_extensions
    test cx, 1                ; bit 0 = extended read/write supported
    jz no_extensions

    ; Read the kernel.
    mov si, disk_address_packet
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13

    ; Carry flag = error.
    jc disk_error


; ============================================================
; DISK DIAGNOSTIC
; ============================================================

    mov si, success_message
    call print_string


; ============================================================
; ENABLE THE A20 LINE
; ============================================================
; The kernel heap lives at 0x100000 (1 MiB). With A20 disabled,
; every address with bit 20 set silently wraps back down to low
; memory, so the heap would alias the IVT / BIOS data / kernel.
; We must not continue if A20 cannot be enabled.
; ============================================================

    call enable_a20
    jc a20_ok

    mov si, a20_error_message
    call print_string
    jmp hang16

a20_ok:


; ============================================================
; PREPARE PROTECTED MODE
; ============================================================
; We are currently still in 16-bit real mode.
;
; Before switching modes, we:
;   1. Disable interrupts
;   2. Load our Global Descriptor Table
;   3. Print a diagnostic
;   4. Set the protected-mode bit in CR0
;   5. Perform a far jump into protected mode
; ============================================================

    cli

    ; Load the Global Descriptor Table.
    lgdt [gdt_descriptor]

    ; This message MUST happen before protected mode.
    ; BIOS video interrupts don't work in protected mode.
    mov si, protected_message
    call print_string

    ; Force VGA into 80x25 colour text mode.
    mov ax, 0x0003
    int 0x10

    ; Enable protected mode.
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; Far jump into the 32-bit code segment.
    jmp dword CODE_SEG:protected_mode


; ============================================================
; DISK ERRORS
; ============================================================

no_extensions:
    mov si, no_ext_message
    call print_string
    jmp hang16

disk_error:
    mov si, error_message
    call print_string

hang16:
    cli
    hlt
    jmp hang16


; ============================================================
; A20 LINE
; ============================================================
; check_a20:  returns CF=1 if A20 is enabled, CF=0 if disabled.
;             Preserves all registers except AX.
;
; Method: the word at 0000:7DFE (physical 0x007DFE) is compared
; with the word at FFFF:7E0E (physical 0x107DFE). With A20 off,
; bit 20 is dropped and both addresses are the SAME byte, so
; changing one changes the other.
; ============================================================

check_a20:
    pushf
    push ds
    push es
    push si
    push di
    cli

    xor ax, ax
    mov es, ax
    dec ax                    ; AX = 0xFFFF
    mov ds, ax
    mov di, 0x7DFE
    mov si, 0x7E0E

    mov ax, [es:di]           ; save the original word
    push ax
    not ax                    ; a value that differs from it
    mov [es:di], ax           ; write it through the low address
    cmp [ds:si], ax           ; did it show up at the high address?
    pop ax
    mov [es:di], ax           ; restore the original word

    mov ax, 1                 ; (MOV does not change the flags)
    jne .decided              ; different -> memory is not aliased -> enabled
    xor ax, ax                ; same -> aliased -> A20 disabled
.decided:
    pop di
    pop si
    pop es
    pop ds
    popf
    shr ax, 1                 ; CF = 1 if enabled
    ret


; ------------------------------------------------------------
; a20_settle: poll check_a20 for a while, because some chipsets
; need a moment to act on the request. CF=1 if A20 came up.
; ------------------------------------------------------------

a20_settle:
    mov cx, 0x2000
.poll:
    call check_a20
    jc .done
    loop .poll                ; falls through with CF=0
.done:
    ret


; ------------------------------------------------------------
; enable_a20: try each method in turn, verifying after each one.
; Returns CF=1 on success, CF=0 if every method failed.
; ------------------------------------------------------------

enable_a20:
    ; Already enabled? (Most BIOSes, including QEMU's, do this.)
    call check_a20
    jc .done

    ; Method 1: BIOS service INT 15h AX=2401h
    mov ax, 0x2401
    int 0x15                  ; BIOS: enable A20
    call a20_settle
    jc .done

    ; Method 2: "fast A20" through system control port 0x92
    in al, 0x92
    test al, 2
    jnz .keyboard_controller  ; bit already set, so this method can't help
    or al, 2                  ; bit 1 = A20 gate
    and al, 0xFE              ; bit 0 = system RESET, must stay 0
    out 0x92, al              ; fast A20 write
    call a20_settle
    jc .done

.keyboard_controller:
    ; Method 3: 8042 keyboard controller output port
    call kbc_enable_a20
    call a20_settle
    jc .done

    clc                       ; every method failed
.done:
    ret


; ------------------------------------------------------------
; 8042 keyboard controller helpers (bounded wait, so a machine
; without a legacy controller cannot hang the boot).
; ------------------------------------------------------------

kbc_wait_input:               ; wait until the input buffer is empty
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 2
    jz .done
    loop .wait
.done:
    ret

; Write 0xDF to the controller output port: A20 gate on, system
; reset line inactive (the same sequence Linux's boot code uses).
kbc_enable_a20:
    call kbc_wait_input
    mov al, 0xD1              ; command: write output port
    out 0x64, al
    call kbc_wait_input
    mov al, 0xDF              ; A20 gate = 1, RESET = 1 (inactive)
    out 0x60, al
    jmp kbc_wait_input        ; (tail call)


; ============================================================
; BIOS STRING PRINTING
; ============================================================
; Prints a null-terminated string using BIOS interrupt 10h.
;
; IMPORTANT:
; This only works in 16-bit real mode.
; DO NOT CALL THIS AFTER ENTERING PROTECTED MODE.
; ============================================================

print_string:
    lodsb

    cmp al, 0
    je .done

    mov ah, 0x0E
    mov bh, 0
    int 0x10

    jmp print_string

.done:
    ret


; ============================================================
; 32-BIT PROTECTED MODE
; ============================================================

bits 32

protected_mode:

    ; ========================================================
    ; TEST POINT: DID THE FAR JUMP REACH PROTECTED_MODE?
    ; ========================================================

    mov word [0xB8000], 0x0741    ; Writes 'A' to VGA memory

    ; ========================================================
    ; NORMAL PROTECTED MODE SETUP
    ; ========================================================

    mov ax, DATA_SEG

    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    ; ========================================================
    ; JUMP TO KERNEL
    ; ========================================================

    mov eax, KERNEL_BASE
    call eax

.hang32:
    cli
    hlt
    jmp .hang32


; ============================================================
; BOOT DRIVE STORAGE
; ============================================================

boot_drive:
    db 0
e820_status:
    db 0


; ============================================================
; DISK ADDRESS PACKET (for INT 13h AH=42h)
; ============================================================
; Layout is fixed by the BIOS extensions specification.
; ============================================================

disk_address_packet:
    db 0x10                   ; packet size (16 bytes)
    db 0                      ; reserved
    dw KERNEL_SECTORS         ; number of sectors to read
    dw 0x0000                 ; destination offset
    dw KERNEL_SEGMENT         ; destination segment
    dq KERNEL_LBA             ; starting LBA


; ============================================================
; BOOT MESSAGES
; ============================================================

e820_error_message:
    db "BIOS E820 MEMORY MAP FAILED.", 13, 10
    db 0

e820_ok_message:
    db "E820 OK.", 13, 10
    db 0

loading_message:
    db "TALONSOS", 13, 10
    db "Loading kernel...", 13, 10
    db 0

success_message:
    db "DISK READ OK.", 13, 10
    db 0

protected_message:
    db "ENTERING PROTECTED MODE...", 13, 10
    db 0

error_message:
    db "DISK READ FAILED.", 13, 10
    db 0

no_ext_message:
    db "NO INT13 EXTENSIONS.", 13, 10
    db 0

a20_error_message:
    db "A20 FAILED.", 13, 10
    db 0


; ============================================================
; GLOBAL DESCRIPTOR TABLE
; ============================================================
; The GDT tells the CPU how memory segments should work
; after entering protected mode.
; ============================================================

gdt_start:

gdt_null:
    dq 0


; ------------------------------------------------------------
; CODE SEGMENT
; ------------------------------------------------------------

gdt_code:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0


; ------------------------------------------------------------
; DATA SEGMENT
; ------------------------------------------------------------

gdt_data:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0


gdt_end:


; ============================================================
; GDT DESCRIPTOR
; ============================================================

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


; Segment selectors
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start


; ============================================================
; BOOT SECTOR PADDING
; ============================================================
; BIOS requires the boot sector to be exactly 512 bytes.
;
; The final two bytes must be AA55.
; ============================================================

%if ($-$$) > 2048
    %error "Stage two exceeds four sectors"
%endif
times 2048-($-$$) db 0
