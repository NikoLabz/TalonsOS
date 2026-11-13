#include "exception.h"

namespace
{
    constexpr unsigned int EXCEPTION_COUNT = 32;
    constexpr unsigned int SCREEN_COLUMNS = 80;
    constexpr unsigned int SCREEN_CELLS = 80 * 25;
    constexpr unsigned char PANIC_COLOUR = 0x4F;

    alignas(16) talons_exception::IdtGate idt[EXCEPTION_COUNT];

    extern "C" void* exception_stub_table[EXCEPTION_COUNT];

    volatile uint16_t* const video =
        reinterpret_cast<volatile uint16_t*>(0xB8000);
    unsigned int outputPosition = 0;

    void clearScreen()
    {
        for (unsigned int i = 0; i < SCREEN_CELLS; ++i)
            video[i] = static_cast<uint16_t>((PANIC_COLOUR << 8) | ' ');

        outputPosition = 0;
    }

    void writeCharacter(char value)
    {
        if (value == '\n')
        {
            outputPosition =
                ((outputPosition / SCREEN_COLUMNS) + 1) * SCREEN_COLUMNS;
            return;
        }

        if (outputPosition < SCREEN_CELLS)
            video[outputPosition++] =
                static_cast<uint16_t>((PANIC_COLOUR << 8) | value);
    }

    void writeString(const char* text)
    {
        for (unsigned int i = 0; text[i] != '\0'; ++i)
            writeCharacter(text[i]);
    }

    void writeHex(uint64_t value)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        writeString("0x");

        bool started = false;
        for (int shift = 60; shift >= 0; shift -= 4)
        {
            const unsigned int digit =
                static_cast<unsigned int>((value >> shift) & 0xF);
            if (digit != 0 || started || shift == 0)
            {
                writeCharacter(digits[digit]);
                started = true;
            }
        }
    }

    void writeDecimal(uint64_t value)
    {
        char digits[20];
        unsigned int count = 0;

        do
        {
            digits[count++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
        while (value != 0 && count < sizeof(digits));

        while (count != 0)
            writeCharacter(digits[--count]);
    }

    [[noreturn]] void halt()
    {
        for (;;)
            asm volatile("cli; hlt");
    }

    [[noreturn]] void reportException(
        const talons_exception::RegisterFrame& frame,
        uint64_t faultAddress)
    {
        asm volatile("cli" ::: "memory");
        clearScreen();

        if (frame.vector == 14)
        {
            writeString(talons_exception::pageFaultUser(frame.errorCode)
                ? "USER PAGE FAULT\nAddress: "
                : "KERNEL PAGE FAULT\nAddress: ");
            writeHex(faultAddress);
            writeString("\nError: ");
            writeHex(frame.errorCode);
            writeString("\nAccess: ");
            writeString(talons_exception::pageFaultPresentViolation(
                frame.errorCode) ? "protection violation" : "not present");
            writeString(", ");
            writeString(talons_exception::pageFaultWrite(frame.errorCode)
                ? "write" : "read");
            writeString(", ");
            writeString(talons_exception::pageFaultUser(frame.errorCode)
                ? "user" : "supervisor");
            if (talons_exception::pageFaultInstructionFetch(frame.errorCode))
                writeString(", instruction fetch");
        }
        else
        {
            writeString("KERNEL EXCEPTION\nVector: ");
            writeDecimal(frame.vector);
            writeString("\nError: ");
            writeHex(frame.errorCode);
            writeString("\nInstruction: ");
            writeHex(frame.instructionPointer);
        }

        writeString("\nSystem halted.");
        halt();
    }
}

extern "C" void exception_init()
{
    for (unsigned int i = 0; i < EXCEPTION_COUNT; ++i)
        idt[i] = talons_exception::makeGate(
            reinterpret_cast<uint64_t>(exception_stub_table[i]));

    const talons_exception::IdtPointer pointer = {
        static_cast<uint16_t>(sizeof(idt) - 1),
        reinterpret_cast<uint64_t>(idt)
    };

    asm volatile("lidt %0" : : "m"(pointer) : "memory");
}

extern "C" [[noreturn]] void exception_dispatch(
    const talons_exception::RegisterFrame* frame,
    uint64_t faultAddress)
{
    reportException(*frame, faultAddress);
}

extern "C" [[noreturn]] void exception_test_trigger_page_fault()
{
    volatile uint64_t* const unmapped =
        reinterpret_cast<volatile uint64_t*>(0x40000000ULL);
    *unmapped = 0x504641554C54ULL;
    halt();
}
