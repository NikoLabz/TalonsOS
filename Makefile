# ============================================================
# TalonsOS build — reconstructed from the source tree.
#
# Verified: boots to "TALONSOS KERNEL ONLINE." + shell prompt
# in QEMU (bootloader -> protected mode -> long mode -> kernel_main).
#
# IMPORTANT: -mcmodel=small, not large/kernel. This kernel loads
# at a fixed low address (0x10000), well within 32-bit range.
# -mcmodel=large generates PC-relative+64-bit-immediate call
# sequences that need GOT-style relocation handling; with a flat
# linker script like the one below, those calls land on garbage
# and the CPU triple-faults on entry to kernel_main. Cost me an
# afternoon to track down — don't relearn this one.
# ============================================================

CXX      := g++
AS       := nasm
LD       := ld
OBJCOPY  := objcopy

BUILD    := build

CXXFLAGS := -m64 -ffreestanding -fno-exceptions -fno-rtti \
            -fno-stack-protector -mno-red-zone -mcmodel=small \
            -mgeneral-regs-only \
            -Wall -Wextra -c
INC      := -Ikernel -Ifilesystem/talonsfs/include

# Host-side tool build (tools/mkTalonsFS) — a NORMAL g++ build, not
# freestanding. It links formatter.cpp/layout.cpp/file_block_device.cpp,
# which use <vector>/<fstream>/<filesystem> and must never be compiled
# with the freestanding CXXFLAGS above.
HOSTCXX      := g++
HOSTCXXFLAGS := -std=c++17 -Wall -Wextra
HOSTINC      := -Ikernel -Ifilesystem/talonsfs/include

# TOTAL_DISK_BLOCKS (32768) * 512 bytes/block. Must match
# kernel/block_device.h — if that value ever changes, update this too.
DISK_IMG_BYTES := 16777216

# Kernel-safe TalonsFS core. directory.cpp / filesystem.cpp /
# formatter.cpp / memory_block_device.cpp / file_block_device.cpp
# are HOST-ONLY (they use <vector>/<algorithm>/<limits> and pull
# in libstdc++) — for the test suite and mkTalonsFS tool, never
# for the kernel build.
TALONSFS_SRCS := node_table bitmap block_allocator node_allocator \
                  node_manager node_data_manager data_block_manager layout \
                  time superblock_checksum

KERNEL_SRCS := kernel block_device filesystem keyboard memory \
                new_delete runtime_support shell terminal vga

KERNEL_OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(KERNEL_SRCS)))
TALONSFS_OBJS := $(addprefix $(BUILD)/talonsfs_,$(addsuffix .o,$(TALONSFS_SRCS)))
ENTRY_OBJ := $(BUILD)/entry.o

KERNEL_BIN := $(BUILD)/kernel.bin
KERNEL_FLAT := $(BUILD)/kernel_flat.bin
BOOT_BIN := $(BUILD)/boot.bin
DISK_IMG := $(BUILD)/disk.img
MKTALONSFS := $(BUILD)/mkTalonsFS
TEST_INTEGRITY := $(BUILD)/test_integrity
TEST_PERMISSIONS := $(BUILD)/test_permissions


TOOL_SRCS := tools/mkTalonsFS.cpp \
             filesystem/talonsfs/src/formatter.cpp \
             filesystem/talonsfs/src/layout.cpp \
             filesystem/talonsfs/src/file_block_device.cpp \
             filesystem/talonsfs/src/superblock_utils.cpp \
             filesystem/talonsfs/src/filesystem.cpp \
             filesystem/talonsfs/src/bitmap.cpp \
             filesystem/talonsfs/src/block_allocator.cpp \
             filesystem/talonsfs/src/node_table.cpp \
             filesystem/talonsfs/src/node_allocator.cpp \
             filesystem/talonsfs/src/node_manager.cpp \
             filesystem/talonsfs/src/node_data_manager.cpp \
             filesystem/talonsfs/src/data_block_manager.cpp \
             filesystem/talonsfs/src/directory.cpp \
             filesystem/talonsfs/src/time.cpp \
	     filesystem/talonsfs/src/file_data_manager.cpp

.PHONY: all run clean test

all: $(DISK_IMG)
test: $(TEST_INTEGRITY)
	./$(TEST_INTEGRITY)
	./$(TEST_PERMISSIONS)
TEST_NODE_MANAGER := $(BUILD)/test_node_manager
TEST_FILESYSTEM := $(BUILD)/test_filesystem
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: kernel/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@ $(INC)

$(BUILD)/talonsfs_%.o: filesystem/talonsfs/src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@ -Ifilesystem/talonsfs/include

$(ENTRY_OBJ): kernel/entry.asm | $(BUILD)
	$(AS) -f elf64 $< -o $@

$(TEST_INTEGRITY): filesystem/talonsfs/src/test_integrity.cpp \
                   filesystem/talonsfs/src/bitmap.cpp \
                   filesystem/talonsfs/src/node_table.cpp \
                   filesystem/talonsfs/src/integrity.cpp \
                   filesystem/talonsfs/src/directory.cpp \
                   filesystem/talonsfs/src/node_data_manager.cpp \
                   filesystem/talonsfs/src/data_block_manager.cpp \
                   filesystem/talonsfs/src/memory_block_device.cpp \
                   filesystem/talonsfs/src/block_allocator.cpp
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_PERMISSIONS): filesystem/talonsfs/src/test_permissions.cpp \
                     filesystem/talonsfs/src/permissions.cpp \
                     filesystem/talonsfs/src/node_manager.cpp \
                     filesystem/talonsfs/src/node_table.cpp \
                     filesystem/talonsfs/src/bitmap.cpp \
                     filesystem/talonsfs/src/time.cpp \
      	             filesystem/talonsfs/src/node_allocator.cpp
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_NODE_MANAGER): filesystem/talonsfs/src/test_node_manager.cpp \
                      filesystem/talonsfs/src/node_manager.cpp \
                      filesystem/talonsfs/src/node_allocator.cpp \
                      filesystem/talonsfs/src/node_table.cpp \
                      filesystem/talonsfs/src/bitmap.cpp \
                      filesystem/talonsfs/src/time.cpp
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_FILESYSTEM): filesystem/talonsfs/src/test_filesystem.cpp \
       		    filesystem/talonsfs/src/layout.cpp \
       	      	    filesystem/talonsfs/src/superblock_utils.cpp \
		    filesystem/talonsfs/src/formatter.cpp \
	            filesystem/talonsfs/src/filesystem.cpp \
                    filesystem/talonsfs/src/file_data_manager.cpp \
                    filesystem/talonsfs/src/directory.cpp \
                    filesystem/talonsfs/src/node_manager.cpp \
                    filesystem/talonsfs/src/node_allocator.cpp \
                    filesystem/talonsfs/src/node_table.cpp \
                    filesystem/talonsfs/src/node_data_manager.cpp \
                    filesystem/talonsfs/src/data_block_manager.cpp \
                    filesystem/talonsfs/src/block_allocator.cpp \
                    filesystem/talonsfs/src/bitmap.cpp \
                    filesystem/talonsfs/src/integrity.cpp \
                    filesystem/talonsfs/src/permissions.cpp \
                    filesystem/talonsfs/src/time.cpp \
                    filesystem/talonsfs/src/file_block_device.cpp
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJS) $(TALONSFS_OBJS) linker.ld
	$(LD) -n -T linker.ld -o $@ $(ENTRY_OBJ) $(KERNEL_OBJS) $(TALONSFS_OBJS)

$(KERNEL_FLAT): $(KERNEL_BIN)
	$(OBJCOPY) -O binary $< $@

$(BOOT_BIN): boot/boot.asm $(KERNEL_FLAT)
	$(eval SECTORS := $(shell echo $$(( ($$(stat -c%s $(KERNEL_FLAT)) + 511) / 512 ))))
	$(AS) -f bin -D KERNEL_SECTORS=$(SECTORS) $< -o $@

# Host-side formatter tool. NOT freestanding — links against the
# host C++ standard library, unlike everything above this line.
$(MKTALONSFS): $(TOOL_SRCS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(TOOL_SRCS) -o $@ $(HOSTINC)

# Boot sector + kernel go in first (bytes 0 .. FILESYSTEM_START*512),
# then mkTalonsFS formats and splices TalonsFS in starting at physical
# block FILESYSTEM_START — see tools/mkTalonsFS.cpp for why this can't
# just be a plain dd of a filesystem image at offset 0.
$(DISK_IMG): $(BOOT_BIN) $(KERNEL_FLAT) $(MKTALONSFS)
	dd if=/dev/zero of=$@ bs=$(DISK_IMG_BYTES) count=1 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ conv=notrunc 2>/dev/null
	dd if=$(KERNEL_FLAT) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	$(MKTALONSFS) $@

run: $(DISK_IMG)
	qemu-system-x86_64 -drive format=raw,file=$(DISK_IMG),if=ide -boot order=c

clean:
	rm -rf $(BUILD)
