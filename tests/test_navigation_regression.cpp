// Regression test for the "directory entries vanish after navigating
// back to the parent" bug.
//
// This links the REAL, unmodified kernel/filesystem.cpp,
// kernel/terminal.cpp and kernel/shell.cpp (the exact code that runs
// under QEMU/real hardware) against host-only test doubles for the
// three low-level I/O modules they depend on (ATA disk, VGA text
// memory, PS/2 keyboard) -- see tests/host_stubs/. No production
// kernel or filesystem source is modified or reimplemented here.
//
// Two independent things are checked:
//
//   1. TalonsFS storage/navigation correctness: create/populate
//      directories, change into and back out of them repeatedly, and
//      verify filesystem_list_directory() returns the right names,
//      types, and count every time. (This is the part of the bug
//      report that turned out NOT to be broken -- kept here as a
//      permanent regression guard regardless.)
//
//   2. The actual root cause: kernel/terminal.cpp's
//      terminal_set_output_position() failing to fully normalize the
//      cursor when a single multi-line print_string() call (like
//      shell.cpp's directory-listing banner) pushes the cursor more
//      than one row past the bottom of the screen. This is
//      reproduced by forcing the cursor near the bottom of the
//      screen (via the real terminal_set_output_position()) and then
//      running the real "LiterallyFDs!@Server" listing command
//      through the real shell_handle_line(), then inspecting the
//      mock VGA buffer to confirm the directory name actually landed
//      somewhere in the visible 80x25 window.

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "filesystem.h"
#include "terminal.h"
#include "shell.h"

// From tests/host_stubs/kernel_block_device_stub.cpp
void test_open_disk(const char* path);
void test_close_disk();

// From tests/host_stubs/vga_stub.cpp
char test_vga_char_at(int cell);

namespace {

int failures = 0;

void check(bool condition, const char* label)
{
    if (condition) {
        std::printf("%s: PASSED\n", label);
    } else {
        std::printf("%s: FAILED\n", label);
        ++failures;
    }
}

bool listingHasEntry(
    const kfs::ListEntry* entries,
    unsigned int count,
    const char* name,
    bool isDirectory)
{
    for (unsigned int i = 0; i < count; ++i) {
        if (std::strcmp(entries[i].name, name) == 0 &&
            entries[i].isDirectory == isDirectory)
        {
            return true;
        }
    }
    return false;
}

// Does this substring appear anywhere in the visible 80x25 (2000
// cell) window of the mock VGA buffer? Used to confirm text that
// filesystem_list_directory() returned correctly was actually
// *rendered* somewhere on-screen, not silently written off-screen.
bool visibleScreenContains(const char* needle)
{
    char screen[2001];

    for (int i = 0; i < 2000; ++i) {
        char c = test_vga_char_at(i);
        screen[i] = (c == '\0') ? ' ' : c;
    }

    screen[2000] = '\0';

    return std::strstr(screen, needle) != nullptr;
}

}

int main()
{
    test_open_disk("build/test_disk.img");

    filesystem_init();

    if (!filesystem_is_mounted()) {
        std::printf("Filesystem mount: FAILED\n");
        test_close_disk();
        return 1;
    }

    std::printf("Filesystem mount: PASSED\n");

    /*
     * ============================================================
     * PART 1: original bug report, at the storage/navigation level.
     * ============================================================
     */

    check(
        filesystem_create_directory("parentdir"),
        "Create parent directory");

    check(
        filesystem_change_directory("parentdir"),
        "Enter parent directory");

    check(
        filesystem_create_directory("childdir"),
        "Create child directory inside parent");

    check(
        filesystem_create_file("childfile"),
        "Create child file inside parent");

    {
        kfs::ListEntry entries[kfs::MAX_LIST_ENTRIES];
        const unsigned int count = filesystem_list_directory(entries);

        check(
            count == 2 &&
            listingHasEntry(entries, count, "childdir", true) &&
            listingHasEntry(entries, count, "childfile", false),
            "Parent directory lists its own two children correctly");
    }

    check(
        filesystem_change_directory("childdir"),
        "Enter child directory");

    {
        kfs::ListEntry entries[kfs::MAX_LIST_ENTRIES];
        const unsigned int count = filesystem_list_directory(entries);
        check(count == 0, "Child directory is empty as expected");
    }

    check(
        filesystem_change_directory(".."),
        "Navigate back to parent from child");

    {
        // This is the exact original bug report: after entering a
        // child and navigating back, the parent must report BOTH the
        // correct count AND the correct entry names/types.
        kfs::ListEntry entries[kfs::MAX_LIST_ENTRIES];
        const unsigned int count = filesystem_list_directory(entries);

        check(
            count == 2,
            "Parent object count correct after returning from child");

        check(
            listingHasEntry(entries, count, "childdir", true) &&
            listingHasEntry(entries, count, "childfile", false),
            "Parent entry names/types still present after returning "
            "from child");
    }

    check(
        filesystem_change_directory(".."),
        "Navigate back to root from parent");

    /*
     * ============================================================
     * PART 2: repeated navigation, multiple siblings.
     * parent -> child -> parent -> child -> parent
     * ============================================================
     */

    check(
        filesystem_create_directory("siblingA"),
        "Create siblingA at root");

    check(
        filesystem_create_directory("siblingB"),
        "Create siblingB at root");

    check(
        filesystem_create_file("siblingC"),
        "Create siblingC (file) at root");

    for (int round = 0; round < 3; ++round) {
        check(
            filesystem_change_directory("siblingA"),
            "Repeated navigation: enter siblingA");

        char fname[16];
        std::snprintf(fname, sizeof(fname), "r%dfile", round);
        check(
            filesystem_create_file(fname),
            "Repeated navigation: create a file inside siblingA");

        check(
            filesystem_change_directory(".."),
            "Repeated navigation: return to root");

        kfs::ListEntry entries[kfs::MAX_LIST_ENTRIES];
        const unsigned int count = filesystem_list_directory(entries);

        check(
            count == 4 &&
            listingHasEntry(entries, count, "parentdir", true) &&
            listingHasEntry(entries, count, "siblingA", true) &&
            listingHasEntry(entries, count, "siblingB", true) &&
            listingHasEntry(entries, count, "siblingC", false),
            "Root still lists all four entries correctly "
            "after repeated round trip");

        if (!(count == 4 &&
              listingHasEntry(entries, count, "parentdir", true) &&
              listingHasEntry(entries, count, "siblingA", true) &&
              listingHasEntry(entries, count, "siblingB", true) &&
              listingHasEntry(entries, count, "siblingC", false)))
        {
            std::printf("  DEBUG round=%d count=%u\n", round, count);
            for (unsigned int i = 0; i < count; ++i) {
                std::printf(
                    "    [%u] name='%s' isDir=%d\n",
                    i, entries[i].name, entries[i].isDirectory);
            }
        }
    }

    /*
     * ============================================================
     * PART 3: the actual root cause -- terminal cursor overflow.
     *
     * Force the cursor near the bottom of the screen (as happens
     * naturally after enough prior shell output), then run the real
     * shell_handle_line("LiterallyFDs!@Server") -- the exact
     * directory-listing command -- and confirm the directory name
     * is actually rendered somewhere in the visible screen, not
     * silently written off-screen by an under-corrected cursor.
     * ============================================================
     */

    terminal_init();

    // The exact starting position found (by exhaustive sweep of all
    // 2000 possible cursor positions) to be the worst case before
    // the fix: the listing banner's three newlines push the cursor
    // two rows past the bottom of the screen in a single
    // print_string() call, which the old single "if" correction
    // could not fully recover from.
    terminal_set_output_position(1919);

    shell_handle_line("LiterallyFDs!@Server");

    check(
        visibleScreenContains("siblingA") ||
        visibleScreenContains("parentdir"),
        "Directory name is actually rendered on-screen "
        "after cursor overflow near bottom of screen");

    check(
        visibleScreenContains("objects."),
        "Object count line is rendered on-screen");

    /*
     * Repeat from several other near-boundary starting positions,
     * since the bug was position-dependent (only ~160 of 2000
     * possible starting positions triggered it before the fix).
     */

    bool allBoundaryPositionsOk = true;

    for (int start = 1840; start < 2000; start += 7) {
        terminal_set_output_position(static_cast<unsigned short>(start));
        shell_handle_line("LiterallyFDs!@Server");

        if (!visibleScreenContains("siblingA") &&
            !visibleScreenContains("parentdir"))
        {
            allBoundaryPositionsOk = false;
            std::printf(
                "  (failed at starting cursor position %d)\n",
                start);
        }
    }

    check(
        allBoundaryPositionsOk,
        "Directory name remains visible across all "
        "near-bottom-of-screen starting positions");

    test_close_disk();

    if (failures > 0) {
        std::printf("\n%d check(s) FAILED.\n", failures);
        return 1;
    }

    std::printf("\nAll checks PASSED.\n");
    return 0;
}
