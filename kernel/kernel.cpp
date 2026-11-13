#include "frame_allocator.h"
#include "paging.h"
#include "paging_internal.h"
#include "e820.h"
#include "standard_streams.h"
#include "memory.h"
#include "filesystem.h"
#include "vga.h"
#include "terminal.h"
#include "block_device.h"

extern "C" char _kernel_end;

namespace
{
    bool paging_runtime_test()
    {
        constexpr uint64_t TEST_ADDRESS = 0xFFFF900000000000ULL;
        constexpr uint64_t MARKER = 0x504147494E475445ULL;
        uint64_t frame = 0;
        if (!talons_paging::mapNewPage(
                TEST_ADDRESS, frame,
                talons_paging::MAP_WRITABLE |
                talons_paging::MAP_NO_EXECUTE))
            return false;

        volatile uint64_t* page =
            reinterpret_cast<volatile uint64_t*>(TEST_ADDRESS);
        bool ok = page[0] == 0 && page[511] == 0;
        page[0] = MARKER;
        ok = ok && page[0] == MARKER;

        talons_paging::PageInfo info{};
        ok = ok && talons_paging::queryPage(TEST_ADDRESS, info) &&
             info.physicalAddress == frame &&
             info.pageSize == talons_paging_internal::PAGE_SIZE;

        if (!talons_paging::unmapPage(TEST_ADDRESS))
            return false; // Keep the mapped frame allocated on cleanup failure.
        return talons_frames::release(frame) && ok;
    }

    bool user_memory_runtime_test()
    {
        using namespace talons_paging;
        const uint64_t kernelRoot = activeAddressSpace();
        const size_t framesBefore = talons_frames::freeFrameCount();
        AddressSpace testSpace{};
        if (!createAddressSpace(testSpace)) return false;

        uint64_t writableFrame = 0;
        uint64_t readOnlyFrame = 0;
        bool ok = mapUserPage(testSpace, USER_BASE, writableFrame,
                              MAP_USER | MAP_WRITABLE | MAP_NO_EXECUTE);
        ok = ok && mapUserPage(testSpace,
                               USER_BASE + talons_paging_internal::PAGE_SIZE,
                               readOnlyFrame, MAP_USER | MAP_NO_EXECUTE);

        PageInfo info{};
        ok = ok && queryAddressSpace(testSpace, USER_BASE, info) &&
             (info.flags & MAP_USER) && (info.flags & MAP_WRITABLE) &&
             (info.flags & MAP_NO_EXECUTE);
        ok = ok && queryAddressSpace(testSpace,
                                     USER_BASE + talons_paging_internal::PAGE_SIZE,
                                     info) &&
             (info.flags & MAP_USER) && !(info.flags & MAP_WRITABLE);
        ok = ok && queryAddressSpace(testSpace, 0x100000, info) &&
             !(info.flags & MAP_USER);

        if (ok && activateAddressSpace(testSpace))
        {
            volatile uint64_t* userPage =
                reinterpret_cast<volatile uint64_t*>(USER_BASE);
            ok = userPage[0] == 0 && userPage[511] == 0;
            userPage[0] = 0x5553455250414745ULL;
            ok = ok && userPage[0] == 0x5553455250414745ULL;
            ok = activateAddressSpace(AddressSpace{kernelRoot, true}) && ok;
        }
        else
        {
            ok = false;
            (void)activateAddressSpace(AddressSpace{kernelRoot, true});
        }

        const bool destroyed = destroyAddressSpace(testSpace);
        return destroyed && ok &&
               talons_frames::freeFrameCount() == framesBefore;
    }
}

extern "C" void kernel_main()
{
    memory_init();

    const bool pagingReady = talons_paging::init();

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
    const bool pagingVmReady =
        pagingReady && framesReady && paging_runtime_test() &&
        user_memory_runtime_test();
    talons_heap::CheckReport heapReport{};
    const bool allocatorIntegrity = framesReady && talons_frames::verify() &&
                                    memory_check(heapReport);

    constexpr unsigned char GREEN = 0x0A;
    constexpr unsigned char RED = 0x0C;

    const char* message = "TALONSOS KERNEL ONLINE";
    for (int i = 0; message[i] != '\0'; ++i)
        vga_putchar_at(i, message[i], GREEN);

    const bool fsInitOk = filesystem_init();
    const bool fsReady = fsInitOk && filesystem_is_mounted();
    const bool taloncReady = fsReady;

    const char* testStatus = "TALONS SYSTEM TESTS: VALID";
    const char* failedStatus = "TALONS SYSTEM TESTS: FAILED - ";

    if (pagingReady && pagingVmReady && allocatorIntegrity &&
        e820Valid && framesReady &&
        fsReady && taloncReady)
    {
        for (int i = 0; testStatus[i] != '\0'; ++i)
            vga_putchar_at(80 + i, testStatus[i], GREEN);
    }
    else
    {
        int column = 80;
        for (int i = 0; failedStatus[i] != '\0'; ++i)
            vga_putchar_at(column++, failedStatus[i], RED);

        bool firstFailure = true;

        auto addFailure = [&](const char* name)
        {
            if (!firstFailure)
            {
                vga_putchar_at(column++, ',', RED);
                vga_putchar_at(column++, ' ', RED);
            }

            for (int i = 0; name[i] != '\0'; ++i)
                vga_putchar_at(column++, name[i], RED);

            firstFailure = false;
        };

        if (!pagingReady) addFailure("Paging");
        if (pagingReady && framesReady && !pagingVmReady)
            addFailure("Paging VM");
        if (framesReady && !allocatorIntegrity)
            addFailure("Memory integrity");
        if (!e820Valid) addFailure("E820");
        if (!framesReady) addFailure("Frames");
        if (!fsReady) addFailure("TalonsFS");
        if (!taloncReady) addFailure("TalonC");
    }

    if (!fsInitOk)
        return;

    standard_streams_init(23);

    terminal_init();
    terminal_run();
}
