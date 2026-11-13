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
            -mgeneral-regs-only -Os \
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
                  superblock_checksum

KERNEL_SRCS := kernel block_device block_cache filesystem file_descriptor keyboard standard_streams \
pipe pipe_manager memory new_delete runtime_support shell terminal editor talonc vga kernel_time e820 frame_allocator paging meminfo heap exception

KERNEL_OBJS := $(addprefix $(BUILD)/,$(addsuffix .o,$(KERNEL_SRCS)))
TALONSFS_OBJS := $(addprefix $(BUILD)/talonsfs_,$(addsuffix .o,$(TALONSFS_SRCS)))
ENTRY_OBJ := $(BUILD)/entry.o

KERNEL_BIN := $(BUILD)/kernel.bin
KERNEL_FLAT := $(BUILD)/kernel_flat.bin
BOOT_BIN := $(BUILD)/boot.bin
STAGE2_BIN := $(BUILD)/stage2.bin
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

.PHONY: all run run-debug clean test size-check

all: $(DISK_IMG)

TEST_NODE_MANAGER := $(BUILD)/test_node_manager
TEST_FILE_DESCRIPTOR := $(BUILD)/test_file_descriptor
TEST_FILESYSTEM := $(BUILD)/test_filesystem
TEST_BLOCK_CACHE := $(BUILD)/test_block_cache
TEST_BLOCK_CACHE_FULL := $(BUILD)/test_block_cache_full
TEST_RTC := $(BUILD)/test_rtc
TEST_E820 := $(BUILD)/test_e820
TEST_FRAME_ALLOCATOR := $(BUILD)/test_frame_allocator
TEST_PAGING := $(BUILD)/test_paging
TEST_EXCEPTION := $(BUILD)/test_exception
TEST_MEMORY := $(BUILD)/test_memory
TEST_MEMORY_MAP := $(BUILD)/test_memory_map
TEST_MEMORY_FORMAT := $(BUILD)/test_memory_format
TEST_MEMINFO := $(BUILD)/test_meminfo
TEST_FRAME_HARDENING := $(BUILD)/test_frame_hardening
TEST_HEAP := $(BUILD)/test_heap
TEST_MEMORY_API := $(BUILD)/test_memory_api
TEST_NEW_DELETE := $(BUILD)/test_new_delete
TEST_NAVIGATION := $(BUILD)/test_navigation_regression
TEST_DISK_IMG := $(BUILD)/test_disk.img

# ------------------------------------------------------------
# The rest of the TalonsFS host-side unit tests: small, self-contained
# tests of one component each (bitmap, allocators, node table,
# directory entries, layout, superblock, formatter, file I/O,
# persistence...). They existed in the tree but were never wired into
# `make test`, so a regression in any of these components could pass
# unnoticed. They link against the full host TalonsFS source set;
# superblock_utils.cpp supplies the checksum symbol here, matching
# mkTalonsFS -- see AUDIT_REPORT.md for why the kernel's own copy,
# superblock_checksum.cpp, must never be linked alongside it.
# ------------------------------------------------------------
TALONSFS_HOST_SRCS := \
	filesystem/talonsfs/src/bitmap.cpp \
	filesystem/talonsfs/src/block_allocator.cpp \
	filesystem/talonsfs/src/talons_file_serialization.cpp \
	filesystem/talonsfs/src/talons_file_parsing.cpp \
	filesystem/talonsfs/src/talons_file_integrity.cpp \
	filesystem/talonsfs/src/talons_file_creation.cpp \
	filesystem/talonsfs/src/data_block_manager.cpp \
	filesystem/talonsfs/src/directory.cpp \
	filesystem/talonsfs/src/file_block_device.cpp \
	filesystem/talonsfs/src/talons_file_checksum.cpp \
	filesystem/talonsfs/src/file_data_manager.cpp \
	filesystem/talonsfs/src/talons_file_validator.cpp \
	filesystem/talonsfs/src/filesystem.cpp \
	filesystem/talonsfs/src/formatter.cpp \
	filesystem/talonsfs/src/integrity.cpp \
	filesystem/talonsfs/src/layout.cpp \
	filesystem/talonsfs/src/memory_block_device.cpp \
	filesystem/talonsfs/src/node_allocator.cpp \
	filesystem/talonsfs/src/node_data_manager.cpp \
	filesystem/talonsfs/src/node_manager.cpp \
	filesystem/talonsfs/src/node_table.cpp \
	filesystem/talonsfs/src/permissions.cpp \
	filesystem/talonsfs/src/superblock_utils.cpp \
	filesystem/talonsfs/src/time.cpp \
	filesystem/talonsfs/src/talons_file_loading.cpp \
	filesystem/talonsfs/src/talons_file_type.cpp

TEST_TALONSFS_UNIT_NAMES := bitmap block_allocator block_allocator_protection \
    block_device data_block_manager directory directory_entry \
    directory_list file_block_device file_data_manager formatter \
    layout node node_allocator node_bitmap node_data_manager \
    node_table persistence root_node superblock superblock_magic \
    talons_file_checksum talons_file_validator talons_file_checksum \
    talons_file_serialization talons_file_parsing talons_file_integrity \
    talons_file_creation talons_file_loading talons_file_type 

TEST_TALONSFS_UNIT_BINS := $(addprefix $(BUILD)/test_,$(TEST_TALONSFS_UNIT_NAMES))

$(BUILD)/test_%: filesystem/talonsfs/src/test_%.cpp $(TALONSFS_HOST_SRCS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) $< $(TALONSFS_HOST_SRCS) -o $@

test: $(TEST_INTEGRITY) $(TEST_PERMISSIONS) $(TEST_FILESYSTEM) \
      $(TEST_BLOCK_CACHE) $(TEST_BLOCK_CACHE_FULL) $(TEST_RTC) $(TEST_E820) $(TEST_FRAME_ALLOCATOR) $(TEST_PAGING) $(TEST_EXCEPTION) \
      $(TEST_MEMORY) $(TEST_MEMORY_MAP) $(TEST_MEMORY_FORMAT) $(TEST_MEMINFO) \
      $(TEST_FRAME_HARDENING) $(TEST_HEAP) $(TEST_MEMORY_API) $(TEST_NEW_DELETE) \
      $(TEST_NAVIGATION) $(TEST_NODE_MANAGER) $(TEST_FILE_DESCRIPTOR) $(TEST_TALONSFS_UNIT_BINS)
	./$(TEST_NODE_MANAGER)
	./$(TEST_FILE_DESCRIPTOR)
	./$(TEST_INTEGRITY)
	./$(TEST_PERMISSIONS)
	./$(TEST_FILESYSTEM)
	./$(TEST_BLOCK_CACHE)
	./$(TEST_BLOCK_CACHE_FULL)
	./$(TEST_RTC)
	./$(TEST_E820)
	./$(TEST_FRAME_ALLOCATOR)
	./$(TEST_PAGING)
	./$(TEST_EXCEPTION)
	./$(TEST_MEMORY)
	./$(TEST_MEMORY_MAP)
	./$(TEST_MEMORY_FORMAT)
	./$(TEST_MEMINFO)
	./$(TEST_FRAME_HARDENING)
	./$(TEST_HEAP)
	./$(TEST_MEMORY_API)
	./$(TEST_NEW_DELETE)
	@# test_navigation_regression mutates its fixture disk in place
	@# (creates directories/files as part of the test). Regenerate it
	@# fresh here, unconditionally, rather than relying on Make's
	@# file-presence tracking for $(TEST_DISK_IMG): once that file
	@# exists from a prior `make test` run, Make considers it up to
	@# date and never rebuilds it, so a second run without `make
	@# clean` in between would reuse a disk already containing
	@# "parentdir", "siblingA", etc. and fail on AlreadyExists --
	@# a false regression report, not a real one.
	rm -f $(TEST_DISK_IMG)
	$(MAKE) $(TEST_DISK_IMG)
	./$(TEST_NAVIGATION)
	@for t in $(TEST_TALONSFS_UNIT_BINS); do \
		echo "--- $$t ---"; \
		./$$t || exit 1; \
	done

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
                   filesystem/talonsfs/src/block_allocator.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_PERMISSIONS): filesystem/talonsfs/src/test_permissions.cpp \
                     filesystem/talonsfs/src/permissions.cpp \
                     filesystem/talonsfs/src/node_manager.cpp \
                     filesystem/talonsfs/src/node_table.cpp \
                     filesystem/talonsfs/src/bitmap.cpp \
                     filesystem/talonsfs/src/time.cpp \
                     filesystem/talonsfs/src/node_allocator.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_FILE_DESCRIPTOR): kernel/file_descriptor_test.cpp \
                        kernel/file_descriptor.cpp \
                        kernel/file_descriptor.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		kernel/file_descriptor_test.cpp \
		kernel/file_descriptor.cpp \
		-o $@

$(TEST_NODE_MANAGER): filesystem/talonsfs/src/test_node_manager.cpp \
                      filesystem/talonsfs/src/node_manager.cpp \
                      filesystem/talonsfs/src/node_allocator.cpp \
                      filesystem/talonsfs/src/node_table.cpp \
                      filesystem/talonsfs/src/bitmap.cpp \
                      filesystem/talonsfs/src/time.cpp | $(BUILD)
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
                    filesystem/talonsfs/src/file_block_device.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

# BlockCache tests -- host-only (MemoryBlockDevice / mock BlockDevice),
# link kernel/block_cache.cpp directly since it is plain freestanding-
# compatible C++ with no kernel-only dependencies.
$(TEST_BLOCK_CACHE): filesystem/talonsfs/src/test_block_cache.cpp \
                     filesystem/talonsfs/src/memory_block_device.cpp \
                     kernel/block_cache.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

$(TEST_BLOCK_CACHE_FULL): filesystem/talonsfs/src/test_block_cache_full.cpp \
                          kernel/block_cache.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		$^ -o $@

# RTC field-conversion test -- pure arithmetic from kernel/rtc_convert.h,
# checked against the host's own timegm(). No CMOS access, no kernel
# sources needed.
$(TEST_RTC): filesystem/talonsfs/src/test_rtc.cpp | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel $< -o $@

# E820 memory-map header and entry validation tests.
$(TEST_E820): kernel/test_e820.cpp kernel/e820.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel $< -o $@

$(TEST_FRAME_ALLOCATOR): kernel/test_frame_allocator.cpp kernel/frame_allocator.cpp kernel/frame_allocator.h kernel/e820.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel kernel/test_frame_allocator.cpp kernel/frame_allocator.cpp -o $@

# Paging translation arithmetic tests; no hardware access required.
$(TEST_PAGING): kernel/test_paging.cpp kernel/paging.cpp kernel/paging.h \
                kernel/paging_internal.h kernel/frame_allocator.cpp \
                kernel/frame_allocator.h kernel/e820.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -DTALONS_PAGING_HOST_TEST -Ikernel \
		kernel/test_paging.cpp kernel/paging.cpp kernel/frame_allocator.cpp -o $@

$(TEST_EXCEPTION): kernel/test_exception.cpp kernel/exception.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel $< -o $@

# Phase 5 memory tests (host-side).
# memory.cpp is built with -DTALONS_HOST_TEST so the kernel heap lives in
# a static buffer instead of physical address 0x100000 (the redesigned
# heap keeps its block headers inside the heap and therefore really
# writes to it). test_memory existed in the tree but was not part of
# `make test`; two of its assertions were changed deliberately in M3.
HEAPDEPS := kernel/heap.cpp kernel/heap.h kernel/memory.cpp kernel/memory.h

$(TEST_MEMORY): kernel/test_memory.cpp $(HEAPDEPS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -DTALONS_HOST_TEST -Ikernel kernel/test_memory.cpp kernel/memory.cpp kernel/heap.cpp -o $@

# Heap core against ordinary buffers (no kernel dependencies).
$(TEST_HEAP): kernel/test_heap.cpp kernel/heap.cpp kernel/heap.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel kernel/test_heap.cpp kernel/heap.cpp -o $@

$(TEST_MEMORY_API): kernel/test_memory_api.cpp $(HEAPDEPS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -DTALONS_HOST_TEST -Ikernel kernel/test_memory_api.cpp kernel/memory.cpp kernel/heap.cpp -o $@

# Links the REAL kernel/new_delete.cpp (it replaces global operator new).
$(TEST_NEW_DELETE): kernel/test_new_delete.cpp kernel/new_delete.cpp $(HEAPDEPS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -DTALONS_HOST_TEST -Ikernel kernel/test_new_delete.cpp kernel/new_delete.cpp kernel/memory.cpp kernel/heap.cpp -o $@

$(TEST_MEMORY_MAP): kernel/test_memory_map.cpp kernel/memory_map.h kernel/e820.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel kernel/test_memory_map.cpp -o $@

$(TEST_MEMORY_FORMAT): kernel/test_memory_format.cpp kernel/memory_format.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel kernel/test_memory_format.cpp -o $@

# Phase 5 M2: this test #includes kernel/frame_allocator.cpp so it can
# corrupt private state and check verify(); do not also link that file.
$(TEST_FRAME_HARDENING): kernel/test_frame_hardening.cpp kernel/frame_allocator.cpp kernel/frame_allocator.h kernel/e820.h | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -Ikernel kernel/test_frame_hardening.cpp -o $@

# Links the REAL meminfo.cpp and frame_allocator.cpp; the E820 accessors,
# stream layer and _kernel_end are stubbed inside the test itself.
$(TEST_MEMINFO): kernel/test_meminfo.cpp kernel/meminfo.cpp kernel/meminfo.h \
                 kernel/frame_allocator.cpp kernel/frame_allocator.h \
                 kernel/memory_map.h kernel/memory_format.h kernel/e820.h \
                 $(HEAPDEPS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) -DTALONS_HOST_TEST -Ikernel kernel/test_meminfo.cpp kernel/meminfo.cpp kernel/frame_allocator.cpp kernel/memory.cpp kernel/heap.cpp -o $@

# Navigation/lifecycle regression test -- links the REAL
# kernel/filesystem.cpp, kernel/terminal.cpp, kernel/shell.cpp and
# kernel/block_cache.cpp (unmodified, exactly what QEMU boots) against
# host test doubles for ATA/VGA/keyboard. Needs a pre-formatted fixture
# disk image, built by the real mkTalonsFS so the fixture matches
# production formatting exactly.
$(TEST_DISK_IMG): $(MKTALONSFS) | $(BUILD)
	dd if=/dev/zero of=$@ bs=1M count=16 2>/dev/null
	./$(MKTALONSFS) $@ >/dev/null

$(TEST_NAVIGATION): tests/test_navigation_regression.cpp \
                    tests/host_stubs/kernel_block_device_stub.cpp \
                    tests/host_stubs/vga_stub.cpp \
                    tests/host_stubs/keyboard_stub.cpp \
                    tests/host_stubs/shell_command_stubs.cpp \
                    kernel/filesystem.cpp \
                    kernel/terminal.cpp \
                    kernel/shell.cpp \
                    kernel/block_cache.cpp \
		    kernel/file_descriptor.cpp \
                    kernel/pipe.cpp \
                    kernel/pipe_manager.cpp \
                    kernel/standard_streams.cpp \
                    filesystem/talonsfs/src/node_table.cpp \
                    filesystem/talonsfs/src/bitmap.cpp \
                    filesystem/talonsfs/src/block_allocator.cpp \
                    filesystem/talonsfs/src/node_allocator.cpp \
                    filesystem/talonsfs/src/node_manager.cpp \
                    filesystem/talonsfs/src/node_data_manager.cpp \
                    filesystem/talonsfs/src/data_block_manager.cpp \
                    filesystem/talonsfs/src/layout.cpp \
                    filesystem/talonsfs/src/time.cpp \
                    filesystem/talonsfs/src/superblock_checksum.cpp \
                    $(TEST_DISK_IMG) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(HOSTINC) \
		tests/test_navigation_regression.cpp \
		tests/host_stubs/kernel_block_device_stub.cpp \
		tests/host_stubs/vga_stub.cpp \
		tests/host_stubs/keyboard_stub.cpp \
		tests/host_stubs/shell_command_stubs.cpp \
		kernel/filesystem.cpp \
		kernel/file_descriptor.cpp \
		kernel/terminal.cpp \
		kernel/shell.cpp \
		kernel/block_cache.cpp \
                    kernel/pipe.cpp \
                    kernel/pipe_manager.cpp \
                kernel/standard_streams.cpp \
		filesystem/talonsfs/src/node_table.cpp \
		filesystem/talonsfs/src/bitmap.cpp \
		filesystem/talonsfs/src/block_allocator.cpp \
		filesystem/talonsfs/src/node_allocator.cpp \
		filesystem/talonsfs/src/node_manager.cpp \
		filesystem/talonsfs/src/node_data_manager.cpp \
		filesystem/talonsfs/src/data_block_manager.cpp \
		filesystem/talonsfs/src/layout.cpp \
		filesystem/talonsfs/src/time.cpp \
		filesystem/talonsfs/src/superblock_checksum.cpp \
		-o $@

$(KERNEL_BIN): $(ENTRY_OBJ) $(KERNEL_OBJS) $(TALONSFS_OBJS) linker.ld
	$(LD) -n -T linker.ld -o $@ $(ENTRY_OBJ) $(KERNEL_OBJS) $(TALONSFS_OBJS)

$(KERNEL_FLAT): $(KERNEL_BIN)
	$(OBJCOPY) -O binary $< $@

$(BOOT_BIN): boot/boot.asm | $(BUILD)
	$(AS) -f bin $< -o $@
	test "$$(stat -c%s $@)" -eq 512

$(STAGE2_BIN): boot/stage2.asm $(KERNEL_FLAT) | $(BUILD)
	$(eval SECTORS := $(shell echo $$(( ($$(stat -c%s $(KERNEL_FLAT)) + 511) / 512 ))))
	$(AS) -f bin -D KERNEL_SECTORS=$(SECTORS) $< -o $@
	test "$$(stat -c%s $@)" -eq 2048

# Host-side formatter tool. NOT freestanding — links against the
# host C++ standard library, unlike everything above this line.
$(MKTALONSFS): $(TOOL_SRCS) | $(BUILD)
	$(HOSTCXX) $(HOSTCXXFLAGS) $(TOOL_SRCS) -o $@ $(HOSTINC)

# Boot sector + kernel go in first (bytes 0 .. FILESYSTEM_START*512),
# then mkTalonsFS formats and splices TalonsFS in starting at physical
# block FILESYSTEM_START — see tools/mkTalonsFS.cpp for why this can't
# just be a plain dd of a filesystem image at offset 0.
$(DISK_IMG): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_FLAT) $(MKTALONSFS)
	dd if=/dev/zero of=$@ bs=$(DISK_IMG_BYTES) count=1 2>/dev/null
	dd if=$(BOOT_BIN) of=$@ conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_FLAT) of=$@ bs=512 seek=5 conv=notrunc 2>/dev/null
	$(MKTALONSFS) $@

# Normal run. The guest is allowed to actually reboot: `Byebye!` pulses the
# 8042 reset line and QEMU restarts the machine as real hardware would.
# Phase 5: verify the kernel still fits (image sectors and _kernel_end).
size-check: $(KERNEL_BIN) $(KERNEL_FLAT)
	tools/check_kernel_limits.sh $(KERNEL_BIN) $(KERNEL_FLAT)

run: $(DISK_IMG)
	qemu-system-x86_64 -drive format=raw,file=$(DISK_IMG),if=ide -boot order=c

# Crash-debugging run. NOTE: -no-reboot (reboot=shutdown) combined with
# -no-shutdown (shutdown=pause) turns ANY guest reset -- including a
# deliberate `Byebye!` reboot -- into "pause the VM", which on screen is
# indistinguishable from a kernel hang. That behaviour is wanted here: it
# freezes the machine at the moment of a triple fault so build/qemu.log can
# be read afterwards. Do NOT use this target to test the reboot command.
run-debug: $(DISK_IMG)
	qemu-system-x86_64 -drive format=raw,file=$(DISK_IMG),if=ide -boot order=c -no-reboot -no-shutdown -d int,cpu_reset -D $(BUILD)/qemu.log

clean:
	rm -rf $(BUILD)
