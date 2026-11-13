#!/bin/sh
# Check the kernel against the two hard limits that Phase 5 must respect.
#
#   usage: tools/check_kernel_limits.sh <kernel.elf> <kernel_flat.bin>
#
#   1. Flat image size: at most MAX_KERNEL_SECTORS (123) 512-byte
#      sectors (boot/stage2.asm). TalonsFS starts at disk block 128.
#   2. _kernel_end (image + .bss): at most 0x80000 (linker.ld ASSERT);
#      the stack and page tables live at 0x90000.
#
# Exit status 1 if either limit is exceeded, 2 on usage/tool errors.

MAX_SECTORS=123
SECTOR_SIZE=512
END_LIMIT=$((0x80000))

if [ $# -ne 2 ] || [ ! -f "$1" ] || [ ! -f "$2" ]; then
    echo "usage: $0 <kernel.elf> <kernel_flat.bin>" >&2
    exit 2
fi

ELF=$1
FLAT=$2

flat_bytes=$(wc -c < "$FLAT" | tr -d ' ')
sectors=$(( (flat_bytes + SECTOR_SIZE - 1) / SECTOR_SIZE ))

end_hex=$(nm "$ELF" | awk '$3 == "_kernel_end" { print $1 }')
if [ -z "$end_hex" ]; then
    echo "error: _kernel_end not found in $ELF" >&2
    exit 2
fi
kernel_end=$((0x$end_hex))

status=0

echo "Kernel image : $flat_bytes bytes, $sectors of $MAX_SECTORS sectors" \
     "($((MAX_SECTORS - sectors)) sectors / $(( (MAX_SECTORS - sectors) * SECTOR_SIZE )) bytes headroom)"
if [ "$sectors" -gt "$MAX_SECTORS" ]; then
    echo "FAIL: kernel image exceeds $MAX_SECTORS sectors" >&2
    status=1
fi

printf "Kernel end   : 0x%X of 0x%X (%d bytes headroom)\n" \
    "$kernel_end" "$END_LIMIT" "$((END_LIMIT - kernel_end))"
if [ "$kernel_end" -gt "$END_LIMIT" ]; then
    echo "FAIL: _kernel_end exceeds 0x80000" >&2
    status=1
fi

[ $status -eq 0 ] && echo "Limits OK"
exit $status
