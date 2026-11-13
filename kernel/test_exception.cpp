#include "exception.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>

int main()
{
    using namespace talons_exception;

    constexpr uint64_t HANDLER = 0x123456789ABCDEF0ULL;
    constexpr IdtGate gate = makeGate(HANDLER);

    static_assert(sizeof(IdtGate) == 16);
    static_assert(sizeof(IdtPointer) == 10);
    static_assert(alignof(IdtGate) == 1);
    static_assert(offsetof(RegisterFrame, vector) == 120);
    static_assert(offsetof(RegisterFrame, errorCode) == 128);
    static_assert(offsetof(RegisterFrame, instructionPointer) == 136);

    assert(gate.offsetLow == 0xDEF0);
    assert(gate.selector == KERNEL_CODE_SELECTOR);
    assert(gate.ist == 0);
    assert(gate.attributes == PRESENT_GATE);
    assert(gate.offsetMiddle == 0x9ABC);
    assert(gate.offsetHigh == 0x12345678);
    assert(gate.reserved == 0);

    // Non-present supervisor read, present supervisor write, user fetch.
    assert(!pageFaultPresentViolation(0));
    assert(!pageFaultWrite(0));
    assert(!pageFaultUser(0));
    assert(!pageFaultInstructionFetch(0));

    constexpr uint64_t PROTECTION_WRITE = 1 | 2;
    assert(pageFaultPresentViolation(PROTECTION_WRITE));
    assert(pageFaultWrite(PROTECTION_WRITE));
    assert(!pageFaultUser(PROTECTION_WRITE));

    constexpr uint64_t USER_FETCH = 1 | 4 | 16;
    assert(pageFaultPresentViolation(USER_FETCH));
    assert(!pageFaultWrite(USER_FETCH));
    assert(pageFaultUser(USER_FETCH));
    assert(pageFaultInstructionFetch(USER_FETCH));

    std::puts("Exception descriptor and page-fault decode tests passed.");
    return 0;
}
