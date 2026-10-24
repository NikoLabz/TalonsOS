; TalonsOS first-stage bootloader.
; Loads stage two from LBA 1 into physical address 0x8000.

bits 16
org 0x7C00

BOOT_DRIVE_ADDR equ 0x7DF0
STAGE2_SEGMENT  equ 0x0800
STAGE2_SECTORS  equ 4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Preserve the BIOS boot drive for stage two.
    mov [BOOT_DRIVE_ADDR], dl

    sti

    ; Check for INT 13h extended disk access.
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error
    test cx, 1
    jz disk_error

    ; Load four sectors from LBA 1 to physical 0x8000.
    mov si, disk_address_packet
    mov ah, 0x42
    mov dl, [BOOT_DRIVE_ADDR]
    int 0x13
    jc disk_error

    ; Stage two is assembled with ORG 0x8000.
    jmp 0x0000:0x8000

disk_error:
    mov si, error_message
.print:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0E
    xor bh, bh
    int 0x10
    jmp .print

.halt:
    cli
    hlt
    jmp .halt

disk_address_packet:
    db 0x10, 0
    dw STAGE2_SECTORS
    dw 0
    dw STAGE2_SEGMENT
    dq 1

error_message:
    db "Stage two disk read failed.", 13, 10, 0

; Keep the boot-drive byte at physical 0x7DF0.
times 0x1F0-($-$$) db 0
boot_drive_saved:
    db 0

times 510-($-$$) db 0
dw 0xAA55
