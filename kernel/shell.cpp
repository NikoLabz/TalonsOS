#include "shell.h"
#include "vga.h"
#include "terminal.h"
#include "filesystem.h"

namespace
{
    const unsigned char colour = 0x07;

    bool command_matches(
        const char* line,
        const char* command,
        const char** argOut)
    {
        int i = 0;
        while (command[i] != '\0')
        {
            if (line[i] != command[i]) return false;
            ++i;
        }

        if (line[i] == '\0')
        {
            *argOut = nullptr;
            return true;
        }

        if (line[i] == ' ')
        {
            int j = i;
            while (line[j] == ' ') ++j;
            *argOut = &line[j];
            return true;
        }

        return false;
    }

    bool copy_trimmed_argument(
        const char* src,
        char* dst,
        unsigned int capacity)
    {
        if (src == nullptr) return false;
        unsigned int length = 0;
        while (src[length] != '\0') ++length;
        while (length > 0 && src[length - 1] == ' ') --length;
        if (length == 0 || length >= capacity) return false;
        for (unsigned int i = 0; i < length; ++i) dst[i] = src[i];
        dst[length] = '\0';
        return true;
    }

    void print_string(const char* text)
    {
        unsigned short position = terminal_get_output_position();
        int i = 0;
        while (text[i] != '\0')
        {
            if (text[i] == '\n')
            {
                position = ((position / 80) + 1) * 80;
                ++i;
                continue;
            }
            vga_putchar_at(position, text[i], colour);
            ++position;
            ++i;
        }
        terminal_set_output_position(position);
    }

    void print_number(unsigned int value)
    {
        char digits[10];
        int count = 0;
        if (value == 0) digits[count++] = '0';
        while (value > 0)
        {
            digits[count++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
        char buffer[11];
        for (int i = 0; i < count; ++i) buffer[i] = digits[count - 1 - i];
        buffer[count] = '\0';
        print_string(buffer);
    }

    void print_bytes(const uint8_t* data, uint64_t length)
    {
        unsigned short position = terminal_get_output_position();
        for (uint64_t i = 0; i < length; ++i)
        {
            const char c = static_cast<char>(data[i]);
            if (c == '\n')
            {
                position = ((position / 80) + 1) * 80;
                continue;
            }
            vga_putchar_at(position, c, colour);
            ++position;
        }
        terminal_set_output_position(position);
    }

    /*
     * Print "<prefix>: <reason>" using the structured error recorded
     * by the last filesystem_* call. Without this the shell threw the
     * error enum away and every failure looked identical.
     */
    void print_fs_error(const char* prefix)
    {
        print_string(prefix);
        print_string(": ");
        print_string(
            filesystem_error_string(
                filesystem_get_last_error()));
        print_string("\n");
    }

    // Append "  reason: <structured error>" after flavour text, so the
    // personality of a message is kept but the cause is still visible.
    void print_fs_reason()
    {
        print_string("  reason: ");
        print_string(
            filesystem_error_string(
                filesystem_get_last_error()));
        print_string("\n");
    }

    static inline void outb(uint16_t port, uint8_t val)
    {
        asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
    }

    static inline uint8_t inb(uint16_t port)
    {
        uint8_t val;
        asm volatile ( "inb %1, %0" : "=a"(val) : "Nd"(port) );
        return val;
    }

    /*
     * Reset the machine.
     *
     * The 8042 keyboard controller only latches a command byte when its
     * input buffer is empty (status port 0x64, bit 1 == 0). Writing 0xFE
     * while the buffer is full silently drops the pulse and the machine
     * keeps running. Drain it first, with a bounded spin so a wedged or
     * absent controller cannot hang the kernel.
     *
     * If the pulse does not take effect, fall back to a deliberate triple
     * fault: load a zero-length IDT and raise an interrupt. The CPU cannot
     * dispatch it, cannot dispatch the resulting #GP, and resets.
     */
    [[noreturn]] static void reboot_machine()
    {
        for (uint32_t i = 0; i < 100000; ++i)
        {
            if ((inb(0x64) & 0x02) == 0)
            {
                break;
            }
        }

        outb(0x64, 0xFE);

        // Give the controller a moment to assert the reset line.
        for (uint32_t i = 0; i < 1000000; ++i)
        {
            asm volatile ("pause");
        }

        // Fallback: forced triple fault.
        struct [[gnu::packed]] { uint16_t limit; uint64_t base; }
            nullIdt = { 0, 0 };

        asm volatile ("lidt %0" : : "m"(nullIdt));
        asm volatile ("int $0x03");

        for (;;)
        {
            asm volatile ("hlt");
        }
    }
}

extern "C" void shell_handle_line(const char* line)
{
    const char* arg = nullptr;

    if (command_matches(line, "LiterallyFDs!@Server", &arg))
    {
	print_string("\n          @Server\n\n");
	if (!filesystem_is_mounted()) { print_string("TalonsFS mount failed\n"); return; }
        kfs::ListEntry entries[kfs::MAX_LIST_ENTRIES];
        const unsigned int count = filesystem_list_directory(entries);
        for (unsigned int i = 0; i < count; ++i)
        {
            print_string(entries[i].isDirectory ? "[DIR]     " : "[FILE]    ");
            print_string(entries[i].name);
            print_string("\n");
        }
        print_string("\n");
        print_number(count);
        print_string(" objects.\n");
    }
    else if (command_matches(line, "MakeItYourself>:(", &arg))
    {
        char name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(arg, name, kfs::MAX_NAME_LEN)) print_string("MakeItYourself>:( : missing directory name\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_create_directory(name)) { print_string("Created: "); print_string(name); print_string("\n"); }
        else print_fs_error("MakeItYourself>:(");
    }
    else if (command_matches(line, "DontTouchMe", &arg))
    {
        char name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(arg, name, kfs::MAX_NAME_LEN)) print_string("DontTouchMe : missing file name\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_create_file(name)) { print_string("Created: "); print_string(name); print_string("\n"); }
        else print_fs_error("DontTouchMe");
    }
    
    else if (command_matches(line, "RandomFZ!@MultigranualRice", &arg))
    {
        char name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(arg, name, kfs::MAX_NAME_LEN)) print_string("RandomFZ!@MultigranualRice : missing name\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_remove(name)) print_string("The rice has been cosumed\n");
        else { print_string("The rice has been cooked beyond operational limits\n"); print_fs_reason(); }
    }

    else if (command_matches(line, "Crazy@protien", &arg))
    {
        if (arg == nullptr) { print_string("Crazy@protien : missing arguments\n"); return; }
        char src[kfs::MAX_NAME_LEN];
        unsigned int i = 0;
        while (arg[i] != ' ' && arg[i] != '\0' && i < kfs::MAX_NAME_LEN - 1) { src[i] = arg[i]; i++; }
        src[i] = '\0';
        while (arg[i] == ' ') i++;
        const char* dst = &arg[i];
        char dst_name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(dst, dst_name, kfs::MAX_NAME_LEN)) print_string("Crazy@protien : missing destination\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_copy(src, dst_name)) { print_string("Copied "); print_string(src); print_string(" to "); print_string(dst_name); print_string("\n"); }
        else print_fs_error("Crazy@protien");
    }
    else if (command_matches(line, "MoveItYourself>:(", &arg))
    {
        if (arg == nullptr) { print_string("MoveItYourself>:( : missing arguments\n"); return; }
        char src[kfs::MAX_NAME_LEN];
        unsigned int i = 0;
        while (arg[i] != ' ' && arg[i] != '\0' && i < kfs::MAX_NAME_LEN - 1) { src[i] = arg[i]; i++; }
        src[i] = '\0';
        while (arg[i] == ' ') i++;
        const char* dst = &arg[i];
        char dst_name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(dst, dst_name, kfs::MAX_NAME_LEN)) print_string("MoveItYourself>:( : missing destination\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_move(src, dst_name)) { print_string("Moved "); print_string(src); print_string(" to "); print_string(dst_name); print_string("\n"); }
        else print_fs_error("MoveItYourself>:(");
    }
    else if (command_matches(line, "KittysAreSoft!", &arg))
    {
        char name[kfs::MAX_NAME_LEN];
        if (!copy_trimmed_argument(arg, name, kfs::MAX_NAME_LEN)) print_string("KittysAreSoft! : missing file name\n");
        else if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else
        {
            static uint8_t fileBuffer[kfs::MAX_FILE_READ_BYTES];
            uint64_t size = 0;
            if (!filesystem_read_file(name, fileBuffer, kfs::MAX_FILE_READ_BYTES, &size)) { print_string("The cat couldnt find the file. Have you triend shaking the box?\n"); print_fs_reason(); }
            else { print_bytes(fileBuffer, size); print_string("\n"); }
        }
    } 
   
    else if (command_matches(line, "huh?", &arg))
{
    char name[kfs::MAX_NAME_LEN];

    if (!copy_trimmed_argument(
            arg,
            name,
            kfs::MAX_NAME_LEN))
    {
        print_string("huh? : missing name\n");
    }
    else if (!filesystem_is_mounted())
    {
        print_string("TalonsFS mount failed\n");
    }
    else if (name[0] == '.' &&
             name[1] == '.' &&
             name[2] == '\0')
    {
        if (filesystem_change_directory(name))
        {
            print_string("heading back!\n");
        }
        else
        {
            print_string("hmmm... nowhere to head back to\n");
        }
    }
    else if (filesystem_change_directory(name))
    {
        print_string("I found something called ");
        print_string(name);
        print_string("\n");
    }
    else
    {
        print_string("hmmm... i cant find it\n");
    }
}

    else if (command_matches(line, "WhyIsItEchoing?@Here", &arg))
    {
        if (arg == nullptr) { print_string("WhyIsItEchoing?@Here : missing arguments\n"); return; }
        char name[kfs::MAX_NAME_LEN];
        unsigned int i = 0;
        while (arg[i] != ' ' && arg[i] != '\0' && i < kfs::MAX_NAME_LEN - 1) { name[i] = arg[i]; i++; }
        name[i] = '\0';
        while (arg[i] == ' ') i++;
        const char* text = &arg[i];
        uint32_t textLen = 0;
        while (text[textLen] != '\0') textLen++;
        // Trim trailing spaces from text if needed, but echo usually keeps them?
        // Prompt says "write supplied text", let's keep it as is.
        if (!filesystem_is_mounted()) print_string("TalonsFS mount failed\n");
        else if (filesystem_write_file(name, reinterpret_cast<const uint8_t*>(text), textLen)) { print_string("Wrote to "); print_string(name); print_string("\n"); }
        else print_fs_error("WhyIsItEchoing?@Here");
    }
   
	else if (command_matches(
             line,
             "UnmountTheEagle!@Filesystem",
             &arg))
{
    if (!filesystem_is_mounted())
    {
        print_string("TalonsFS is already unmounted\n");
    }
    else if (filesystem_unmount())
    {
        print_string("TalonsFS unmounted successfully!\n");
    }
    else
    {
        print_string("TalonsFS unmount failed!\n");
    }
}

	else if (command_matches(line, "Byebye!", &arg))
    {
        /*
         * The BlockCache is write-back, so a bare reboot discards every
         * dirty entry. Unmount (which flushes the cache to the platter)
         * before resetting, and say so if it fails -- silently rebooting
         * over unflushed metadata is how filesystems get corrupted.
         */
        if (filesystem_is_mounted())
        {
            if (!filesystem_unmount())
            {
                print_string("WARNING: unmount failed; cached data may be lost\n");
            }
        }

        print_string("Rebooting...\n");
        reboot_machine();
    }
   
        else if (command_matches(line, "FormatTheEagle!@Disk", &arg))
    {
        print_string("Format the entire TalonsFS filesystem? [y/n]\n");

        char confirmation[8];

        if (!copy_trimmed_argument(
                arg,
                confirmation,
                sizeof(confirmation)))
        {
            print_string("Formatting cancelled.\n");
        }
        else if (confirmation[0] == 'y' &&
                 confirmation[1] == '\0')
        {
            print_string("Formatting TalonsFS...\n");

            if (filesystem_format())
            {
                print_string("TalonsFS formatted successfully!\n");
            }
            else
            {
                print_string("TalonsFS formatting failed!\n");
            }
        }
        else
        {
            print_string("Formatting cancelled.\n");
        }
    }

	else
    {
        print_string("Thats not a command buddy!\n");
    }
}
