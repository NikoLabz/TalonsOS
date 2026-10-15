#include "talonc.h"
#include "filesystem.h"
#include "standard_streams.h"
#include "memory.h"

#include <stdint.h>

namespace
{
    constexpr uint32_t SOURCE_CAPACITY = 16384;

    bool is_space(char c)
    {
        return c == ' ' || c == '\t' ||
               c == '\r' || c == '\n';
    }

    void print_error(int fd, const char* message)
    {
        stream_write_string(fd, message);
    }

    bool expect_text(
        const char*& p,
        const char* expected)
    {
        while (*expected != '\0')
        {
            if (*p != *expected)
                return false;
            ++p;
            ++expected;
        }
        return true;
    }

    void skip_space(const char*& p)
    {
        while (is_space(*p))
            ++p;
    }

    bool parse_and_run(
        char* source,
        int stdoutFd,
        int stderrFd)
    {
        const char* p = source;
        skip_space(p);

        if (!expect_text(p, "#%OS"))
        {
            print_error(stderrFd,
                "TalonC: expected #%OS at the start.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "////"))
        {
            print_error(stderrFd,
                "TalonC: expected opening //// marker.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "start.innit=True"))
        {
            print_error(stderrFd,
                "TalonC: expected start.innit=True.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "if.start=True"))
        {
            print_error(stderrFd,
                "TalonC: expected if.start=True.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "&&"))
        {
            print_error(stderrFd,
                "TalonC: expected && in startup check.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "innit"))
        {
            print_error(stderrFd,
                "TalonC: expected innit in startup check.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "!4"))
        {
            print_error(stderrFd,
                "TalonC: expected !4 in startup check.\n");
            return false;
        }

        skip_space(p);
        if (!expect_text(p, "begin{!"))
        {
            print_error(stderrFd,
                "TalonC: expected begin{! main block.\n");
            return false;
        }

        for (;;)
        {
            skip_space(p);

            if (expect_text(p, "?}"))
                break;

            if (*p == '\0')
            {
                print_error(stderrFd,
                    "TalonC: missing ?} main-block terminator.\n");
                return false;
            }

            if (!expect_text(p, "*say"))
            {
                print_error(stderrFd,
                    "TalonC: unsupported statement; only *say is supported.\n");
                return false;
            }

            if (*p != '"')
            {
                print_error(stderrFd,
                    "TalonC: expected a quoted string after *say.\n");
                return false;
            }
            ++p;

            const char* message = p;
            while (*p != '\0' && *p != '"' &&
                   *p != '\n' && *p != '\r')
            {
                if (*p == '\\')
                {
                    print_error(stderrFd,
                        "TalonC: escape sequences are not supported yet.\n");
                    return false;
                }
                ++p;
            }

            if (*p != '"')
            {
                print_error(stderrFd,
                    "TalonC: unterminated or multiline string.\n");
                return false;
            }

            const char* messageEnd = p;
            ++p;
            skip_space(p);

            if (*p != ';')
            {
                print_error(stderrFd,
                    "TalonC: expected semicolon after *say.\n");
                return false;
            }
            ++p;

            for (const char* ch = message; ch < messageEnd; ++ch)
                stream_write_char(stdoutFd, *ch);
            stream_write_char(stdoutFd, '\n');
        }

        skip_space(p);
        if (!expect_text(p, "////"))
        {
            print_error(stderrFd,
                "TalonC: expected closing //// marker.\n");
            return false;
        }

        skip_space(p);
        if (*p != '\0')
        {
            print_error(stderrFd,
                "TalonC: unexpected text after closing marker.\n");
            return false;
        }

        print_error(stdoutFd,
            "TalonC: program completed successfully.\n");
        return true;
    }
}

void talonc_run(const char* filename, int stdoutFd, int stderrFd)
{
    if (filename == nullptr || filename[0] == '\0')
    {
        print_error(stderrFd, "Usage: talonc <filename.tal>\n");
        return;
    }

    char* buffer = static_cast<char*>(kmalloc(SOURCE_CAPACITY + 1));
    if (buffer == nullptr)
    {
        print_error(stderrFd, "TalonC: not enough kernel heap memory.\n");
        return;
    }

    uint64_t size = 0;
    if (!filesystem_read_file(
            filename,
            reinterpret_cast<uint8_t*>(buffer),
            SOURCE_CAPACITY,
            &size))
    {
        kfree(buffer);
        print_error(stderrFd,
            "TalonC: cannot read source file or file exceeds 16 KiB.\n");
        return;
    }

    if (size > SOURCE_CAPACITY)
    {
        kfree(buffer);
        print_error(stderrFd, "TalonC: source file too large.\n");
        return;
    }

    buffer[size] = '\0';
    parse_and_run(buffer, stdoutFd, stderrFd);
    kfree(buffer);
}
