#pragma once

#include <cstdint>

namespace talons_exception
{
    constexpr uint8_t PRESENT_GATE = 0x8E;
    constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;

    struct IdtGate
    {
        uint16_t offsetLow;
        uint16_t selector;
        uint8_t ist;
        uint8_t attributes;
        uint16_t offsetMiddle;
        uint32_t offsetHigh;
        uint32_t reserved;
    } __attribute__((packed));

    struct IdtPointer
    {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    struct RegisterFrame
    {
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
        uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
        uint64_t vector;
        uint64_t errorCode;
        uint64_t instructionPointer;
        uint64_t codeSegment;
        uint64_t flags;
    };

    constexpr IdtGate makeGate(uint64_t handler)
    {
        return {
            static_cast<uint16_t>(handler),
            KERNEL_CODE_SELECTOR,
            0,
            PRESENT_GATE,
            static_cast<uint16_t>(handler >> 16),
            static_cast<uint32_t>(handler >> 32),
            0
        };
    }

    constexpr bool pageFaultPresentViolation(uint64_t errorCode)
    {
        return (errorCode & 1) != 0;
    }

    constexpr bool pageFaultWrite(uint64_t errorCode)
    {
        return (errorCode & 2) != 0;
    }

    constexpr bool pageFaultUser(uint64_t errorCode)
    {
        return (errorCode & 4) != 0;
    }

    constexpr bool pageFaultInstructionFetch(uint64_t errorCode)
    {
        return (errorCode & 16) != 0;
    }
}
