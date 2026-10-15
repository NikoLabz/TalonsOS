#include "shell.h"
#include "editor.h"
#include "talonc.h"
#include "vga.h"
#include "terminal.h"
#include "filesystem.h"
#include "standard_streams.h"
#include "file_descriptor.h"

namespace
{
    constexpr unsigned int SHELL_LINE_MAX = 512;
    constexpr unsigned int MAX_COMMANDS = 4;

    struct CommandContext
    {
        int stdinFd;
        int stdoutFd;
        int stderrFd;
    };

    struct ParsedCommand
    {
        char command[SHELL_LINE_MAX];

        bool hasInputRedirect;
        bool hasOutputRedirect;

        char inputFile[kfs::MAX_NAME_LEN];
        char outputFile[kfs::MAX_NAME_LEN];
    };

    bool command_matches(
        const char* line,
        const char* command,
        const char** argOut)
    {
        unsigned int i = 0;

        while (command[i] != '\0')
        {
            if (line[i] != command[i])
                return false;

            ++i;
        }

        if (line[i] == '\0')
        {
            *argOut = nullptr;
            return true;
        }

        if (line[i] == ' ')
        {
            unsigned int j = i;

            while (line[j] == ' ')
                ++j;

            *argOut = &line[j];
            return true;
        }

        return false;
    }

    unsigned int string_length(
        const char* string)
    {
        if (string == nullptr)
            return 0;

        unsigned int length = 0;

        while (string[length] != '\0')
            ++length;

        return length;
    }

    void copy_string(
        const char* source,
        char* destination,
        unsigned int capacity)
    {
        if (destination == nullptr ||
            capacity == 0)
        {
            return;
        }

        if (source == nullptr)
        {
            destination[0] = '\0';
            return;
        }

        unsigned int i = 0;

        while (source[i] != '\0' &&
               i + 1 < capacity)
        {
            destination[i] = source[i];
            ++i;
        }

        destination[i] = '\0';
    }

    bool copy_trimmed_argument(
        const char* src,
        char* dst,
        unsigned int capacity)
    {
        if (src == nullptr ||
            dst == nullptr ||
            capacity == 0)
        {
            return false;
        }

        unsigned int length = 0;

        while (src[length] != '\0')
            ++length;

        while (length > 0 &&
               src[length - 1] == ' ')
        {
            --length;
        }

        if (length == 0 ||
            length >= capacity)
        {
            return false;
        }

        for (unsigned int i = 0;
             i < length;
             ++i)
        {
            dst[i] = src[i];
        }

        dst[length] = '\0';

        return true;
    }

    void stream_print_string(
        int fd,
        const char* text)
    {
        stream_write_string(
            fd,
            text
        );
    }

    void stream_print_number(
        int fd,
        uint64_t value)
    {
        char digits[21];
        unsigned int count = 0;

        if (value == 0)
        {
            stream_write_char(
                fd,
                '0'
            );

            return;
        }

        while (value > 0 &&
               count < sizeof(digits))
        {
            digits[count++] =
                static_cast<char>(
                    '0' + (value % 10)
                );

            value /= 10;
        }

        while (count > 0)
        {
            --count;

            stream_write_char(
                fd,
                digits[count]
            );
        }
    }

    void stream_print_bytes(
        int fd,
        const uint8_t* data,
        uint64_t length)
    {
        if (data == nullptr)
            return;

        stream_write(
            fd,
            data,
            static_cast<uint32_t>(length)
        );
    }

    void print_fs_error(
        const CommandContext& context,
        const char* prefix)
    {
        stream_print_string(
            context.stderrFd,
            prefix
        );

        stream_print_string(
            context.stderrFd,
            ": "
        );

        stream_print_string(
            context.stderrFd,
            filesystem_error_string(
                filesystem_get_last_error()
            )
        );

        stream_print_string(
            context.stderrFd,
            "\n"
        );
    }

    void print_fs_reason(
        const CommandContext& context)
    {
        stream_print_string(
            context.stderrFd,
            "  reason: "
        );

        stream_print_string(
            context.stderrFd,
            filesystem_error_string(
                filesystem_get_last_error()
            )
        );

        stream_print_string(
            context.stderrFd,
            "\n"
        );
    }

    bool parse_unsigned(
        const char* text,
        unsigned int& value)
    {
        if (text == nullptr ||
            text[0] == '\0')
        {
            return false;
        }

        unsigned int result = 0;
        unsigned int i = 0;
        bool foundDigit = false;

        while (text[i] >= '0' &&
               text[i] <= '9')
        {
            foundDigit = true;

            result =
                (result * 10) +
                static_cast<unsigned int>(
                    text[i] - '0'
                );

            ++i;
        }

        if (!foundDigit)
            return false;

        value = result;
        return true;
    }

    bool parse_uint64(
        const char* text,
        uint64_t& value)
    {
        if (text == nullptr ||
            text[0] == '\0')
        {
            return false;
        }

        uint64_t result = 0;
        unsigned int i = 0;
        bool foundDigit = false;

        while (text[i] >= '0' &&
               text[i] <= '9')
        {
            foundDigit = true;

            result =
                (result * 10) +
                static_cast<uint64_t>(
                    text[i] - '0'
                );

            ++i;
        }

        if (!foundDigit)
            return false;

        value = result;
        return true;
    }

    void trim_spaces(
        char* text)
    {
        if (text == nullptr)
            return;

        unsigned int start = 0;

        while (text[start] == ' ')
            ++start;

        if (start != 0)
        {
            unsigned int i = 0;

            while (text[start + i] != '\0')
            {
                text[i] = text[start + i];
                ++i;
            }

            text[i] = '\0';
        }

        unsigned int length =
            string_length(text);

        while (length > 0 &&
               text[length - 1] == ' ')
        {
            text[length - 1] = '\0';
            --length;
        }
    }

    bool split_pipeline(
        char* line,
        char commandParts[MAX_COMMANDS][SHELL_LINE_MAX],
        unsigned int& commandCount)
    {
        commandCount = 0;

        char* start = line;

        while (true)
        {
            if (commandCount >= MAX_COMMANDS)
                return false;

            char* pipe = nullptr;

            unsigned int i = 0;

            while (start[i] != '\0')
            {
                if (start[i] == '|')
                {
                    pipe = &start[i];
                    break;
                }

                ++i;
            }

            if (pipe != nullptr)
            {
                *pipe = '\0';
            }

            trim_spaces(start);

            if (start[0] == '\0')
                return false;

            copy_string(
                start,
                commandParts[commandCount],
                SHELL_LINE_MAX
            );

            ++commandCount;

            if (pipe == nullptr)
                break;

            start = pipe + 1;
        }

        return true;
    }

    bool parse_command(
        const char* source,
        ParsedCommand& parsed)
    {
        parsed.hasInputRedirect = false;
        parsed.hasOutputRedirect = false;

        parsed.command[0] = '\0';
        parsed.inputFile[0] = '\0';
        parsed.outputFile[0] = '\0';

        char buffer[SHELL_LINE_MAX];

        copy_string(
            source,
            buffer,
            sizeof(buffer)
        );

        trim_spaces(buffer);

        if (buffer[0] == '\0')
            return false;

        /*
         * Redirection operators are intentionally recognized only when
         * separated from the command/argument by whitespace.
         *
         * This matters because TalonsOS has commands such as:
         *
         *     MakeItYourself>:( 
         *
         * The '>' inside that command name is NOT shell redirection.
         */

        unsigned int length =
            string_length(buffer);

        for (unsigned int i = 0;
             i < length;
             ++i)
        {
            if (buffer[i] != '<' &&
                buffer[i] != '>')
            {
                continue;
            }

            if (i == 0 ||
                buffer[i - 1] != ' ')
            {
                continue;
            }

            const char operatorCharacter =
                buffer[i];

            buffer[i] = '\0';

            trim_spaces(buffer);

            unsigned int j = i + 1;

            while (source != nullptr &&
                   j < length &&
                   buffer[j] == ' ')
            {
                ++j;
            }

            if (j >= length ||
                buffer[j] == '\0')
            {
                return false;
            }

            unsigned int filenameStart = j;

            while (j < length &&
                   buffer[j] != ' ')
            {
                ++j;
            }

            buffer[j] = '\0';

            if (filenameStart >= length)
                return false;

            if (operatorCharacter == '<')
            {
                if (parsed.hasInputRedirect)
                    return false;

                parsed.hasInputRedirect = true;

                if (!copy_trimmed_argument(
                        &buffer[filenameStart],
                        parsed.inputFile,
                        kfs::MAX_NAME_LEN))
                {
                    return false;
                }
            }
            else
            {
                if (parsed.hasOutputRedirect)
                    return false;

                parsed.hasOutputRedirect = true;

                if (!copy_trimmed_argument(
                        &buffer[filenameStart],
                        parsed.outputFile,
                        kfs::MAX_NAME_LEN))
                {
                    return false;
                }
            }

            /*
             * Redirection is required to be at the end of a command
             * for this first shell parser.
             */
            while (j < length)
            {
                if (buffer[j] != ' ')
                    return false;

                ++j;
            }

            break;
        }

        trim_spaces(buffer);

        copy_string(
            buffer,
            parsed.command,
            sizeof(parsed.command)
        );

        return parsed.command[0] != '\0';
    }

    bool read_stream_until_end(
        int fd,
        uint8_t* buffer,
        uint32_t capacity,
        uint32_t& bytesRead)
    {
        bytesRead = 0;

        if (buffer == nullptr ||
            capacity == 0)
        {
            return false;
        }

        while (bytesRead < capacity)
        {
            const int result =
                stream_read(
                    fd,
                    &buffer[bytesRead],
                    1
                );

            if (result < 0)
            {
                /*
                 * A pipe returns failure when it has no currently
                 * available bytes. For our first non-blocking shell
                 * implementation that means EOF/no more data.
                 */
                break;
            }

            if (result == 0)
                break;

            ++bytesRead;
        }

        return true;
    }

    int open_input_redirect(
        const char* filename)
    {
        if (!filesystem_is_mounted())
            return -1;

        return filesystem_open(
            filename,
            FILESYSTEM_OPEN_READ
        );
    }

    int open_output_redirect(
        const char* filename)
    {
        if (!filesystem_is_mounted())
            return -1;

        /*
         * '>' means replace the destination.
         *
         * TalonsFS currently has no dedicated truncate operation, so
         * remove an existing file and recreate it. If it does not exist,
         * create it directly.
         */
        int existingFd =
            filesystem_open(
                filename,
                FILESYSTEM_OPEN_WRITE
            );

        if (existingFd >= 0)
        {
            filesystem_close(existingFd);

            if (!filesystem_remove(filename))
                return -1;
        }

        if (!filesystem_create_file(filename))
            return -1;

        return filesystem_open(
            filename,
            FILESYSTEM_OPEN_WRITE
        );
    }

    /*
     * Execute one command using the supplied standard streams.
     *
     * Returns true when the command name was recognized.
     */
    bool execute_command(
        const char* line,
        const CommandContext& context)
    {
        const char* arg = nullptr;

        if (command_matches(line, "talonc", &arg))
        {
            if (arg == nullptr || arg[0] == '\0')
                stream_print_string(context.stderrFd,
                    "Usage: talonc <filename.tal>\\n");
            else
                talonc_run(arg, context.stdoutFd, context.stderrFd);
            return true;
        }

        if (command_matches(line, "talonedit", &arg))
        {
            if (arg == nullptr || arg[0] == '\0')
            {
                stream_print_string(
                    context.stderrFd,
                    "Usage: talonedit <filename>\n"
                );
            }
            else
            {
                talonedit_run(arg);
            }

            return true;
        }

        if (command_matches(
                line,
                "LiterallyFDs!@Server",
                &arg))
        {
            stream_print_string(
                context.stdoutFd,
                "\n          @Server\n\n"
            );

            if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );

                return true;
            }

            kfs::ListEntry entries[
                kfs::MAX_LIST_ENTRIES
            ];

            const unsigned int count =
                filesystem_list_directory(
                    entries
                );

            for (unsigned int i = 0;
                 i < count;
                 ++i)
            {
                stream_print_string(
                    context.stdoutFd,
                    entries[i].isDirectory
                        ? "[DIR]     "
                        : "[FILE]    "
                );

                stream_print_string(
                    context.stdoutFd,
                    entries[i].name
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }

            stream_print_string(
                context.stdoutFd,
                "\n"
            );

            stream_print_number(
                context.stdoutFd,
                count
            );

            stream_print_string(
                context.stdoutFd,
                " objects.\n"
            );

            return true;
        }

        if (command_matches(
                line,
                "MakeItYourself>:(",
                &arg))
        {
            char name[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    arg,
                    name,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "MakeItYourself>:( : missing directory name\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_create_directory(name))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Created: "
                );

                stream_print_string(
                    context.stdoutFd,
                    name
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "MakeItYourself>:("
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "DontTouchMe",
                &arg))
        {
            char name[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    arg,
                    name,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "DontTouchMe : missing file name\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_create_file(name))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Created: "
                );

                stream_print_string(
                    context.stdoutFd,
                    name
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "DontTouchMe"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "RandomFZ!@MultigranualRice",
                &arg))
        {
            char name[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    arg,
                    name,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "RandomFZ!@MultigranualRice : missing name\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_remove(name))
            {
                stream_print_string(
                    context.stdoutFd,
                    "The rice has been cosumed\n"
                );
            }
            else
            {
                stream_print_string(
                    context.stderrFd,
                    "The rice has been cooked beyond operational limits\n"
                );

                print_fs_reason(context);
            }

            return true;
        }

        if (command_matches(
                line,
                "Crazy@protien",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "Crazy@protien : missing arguments\n"
                );

                return true;
            }

            char src[kfs::MAX_NAME_LEN];

            unsigned int i = 0;

            while (arg[i] != ' ' &&
                   arg[i] != '\0' &&
                   i < kfs::MAX_NAME_LEN - 1)
            {
                src[i] = arg[i];
                ++i;
            }

            src[i] = '\0';

            while (arg[i] == ' ')
                ++i;

            const char* dst = &arg[i];

            char dstName[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    dst,
                    dstName,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "Crazy@protien : missing destination\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_copy(
                         src,
                         dstName))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Copied "
                );

                stream_print_string(
                    context.stdoutFd,
                    src
                );

                stream_print_string(
                    context.stdoutFd,
                    " to "
                );

                stream_print_string(
                    context.stdoutFd,
                    dstName
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "Crazy@protien"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "MoveItYourself>:(",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "MoveItYourself>:( : missing arguments\n"
                );

                return true;
            }

            char src[kfs::MAX_NAME_LEN];

            unsigned int i = 0;

            while (arg[i] != ' ' &&
                   arg[i] != '\0' &&
                   i < kfs::MAX_NAME_LEN - 1)
            {
                src[i] = arg[i];
                ++i;
            }

            src[i] = '\0';

            while (arg[i] == ' ')
                ++i;

            const char* dst = &arg[i];

            char dstName[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    dst,
                    dstName,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "MoveItYourself>:( : missing destination\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_move(
                         src,
                         dstName))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Moved "
                );

                stream_print_string(
                    context.stdoutFd,
                    src
                );

                stream_print_string(
                    context.stdoutFd,
                    " to "
                );

                stream_print_string(
                    context.stdoutFd,
                    dstName
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "MoveItYourself>:("
                );
            }

            return true;
        }

                if (command_matches(
                line,
                "KittysAreSoft!",
                &arg))
        {
            /*
             * With no filename, copy stdin to stdout.
             *
             * This supports:
             *
             *     command | KittysAreSoft!
             *     KittysAreSoft! < file.tal
             */
            if (arg == nullptr)
            {
                uint8_t buffer[256];

                for (;;)
                {
                    const int bytesRead = stream_read(
                        context.stdinFd,
                        buffer,
                        sizeof(buffer)
                    );

                    if (bytesRead <= 0)
                    {
                        break;
                    }

                    if (!stream_write(
                            context.stdoutFd,
                            buffer,
                            static_cast<uint32_t>(bytesRead)))
                    {
                        stream_print_string(
                            context.stderrFd,
                            "The cat couldnt write stdout.\n"
                        );

                        return true;
                    }
                }

                return true;
            }

            char name[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    arg,
                    name,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "KittysAreSoft! : missing file name\n"
                );

                return true;
            }

            if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );

                return true;
            }

            const int fd = filesystem_open(
    name,
    FILESYSTEM_OPEN_READ
);

if (fd < 0)

            {
                stream_print_string(
                    context.stderrFd,
                    "The cat couldnt find the file. Have you triend shaking the box?\n"
                );

                print_fs_reason(context);
                return true;
            }

            uint8_t buffer[256];

            for (;;)
            {
                const int bytesRead = stream_read(
                    fd,
                    buffer,
                    sizeof(buffer)
                );

                if (bytesRead <= 0)
                {
                    break;
                }

                if (!stream_write(
                        context.stdoutFd,
                        buffer,
                        static_cast<uint32_t>(bytesRead)))
                {
                    filesystem_close(fd);

                    stream_print_string(
                        context.stderrFd,
                        "The cat couldnt write stdout.\n"
                    );

                    return true;
                }
            }

            filesystem_close(fd);

            stream_print_string(
                context.stdoutFd,
                "\n"
            );

            return true;
        }

        if (command_matches(
                line,
                "SesameSeeds",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "SesameSeeds : usage: SesameSeeds <file> <read|write|rw>\n"
                );

                return true;
            }

            char name[kfs::MAX_NAME_LEN];

            unsigned int i = 0;

            while (arg[i] != ' ' &&
                   arg[i] != '\0' &&
                   i < kfs::MAX_NAME_LEN - 1)
            {
                name[i] = arg[i];
                ++i;
            }

            name[i] = '\0';

            while (arg[i] == ' ')
                ++i;

            const char* mode = &arg[i];

            uint32_t flags = 0;

            if (mode[0] == 'r' &&
                mode[1] == 'e' &&
                mode[2] == 'a' &&
                mode[3] == 'd' &&
                mode[4] == '\0')
            {
                flags = FILESYSTEM_OPEN_READ;
            }
            else if (mode[0] == 'w' &&
                     mode[1] == 'r' &&
                     mode[2] == 'i' &&
                     mode[3] == 't' &&
                     mode[4] == 'e' &&
                     mode[5] == '\0')
            {
                flags = FILESYSTEM_OPEN_WRITE;
            }
            else if (mode[0] == 'r' &&
                     mode[1] == 'w' &&
                     mode[2] == '\0')
            {
                flags =
                    FILESYSTEM_OPEN_READ |
                    FILESYSTEM_OPEN_WRITE;
            }
            else
            {
                stream_print_string(
                    context.stderrFd,
                    "SesameSeeds : mode must be read, write, or rw\n"
                );

                return true;
            }

            if (name[0] == '\0')
            {
                stream_print_string(
                    context.stderrFd,
                    "SesameSeeds : missing file name\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else
            {
                const int fd =
                    filesystem_open(
                        name,
                        flags
                    );

                if (fd < 0)
                {
                    print_fs_error(
                        context,
                        "SesameSeeds"
                    );
                }
                else
                {
                    stream_print_string(
                        context.stdoutFd,
                        "Opened "
                    );

                    stream_print_string(
                        context.stdoutFd,
                        name
                    );

                    stream_print_string(
                        context.stdoutFd,
                        " with fd "
                    );

                    stream_print_number(
                        context.stdoutFd,
                        static_cast<uint64_t>(fd)
                    );

                    stream_print_string(
                        context.stdoutFd,
                        "\n"
                    );
                }
            }

            return true;
        }

        if (command_matches(
                line,
                "CloseSesame",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "CloseSesame : missing file descriptor\n"
                );

                return true;
            }

            unsigned int fdValue = 0;

            if (!parse_unsigned(
                    arg,
                    fdValue))
            {
                stream_print_string(
                    context.stderrFd,
                    "CloseSesame : invalid file descriptor\n"
                );

                return true;
            }

            const int fd =
                static_cast<int>(fdValue);

            if (filesystem_close(fd))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Closed fd "
                );

                stream_print_number(
                    context.stdoutFd,
                    fdValue
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "CloseSesame"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "ReadSesame",
                &arg))
        {
            if (arg == nullptr)
            {
                /*
                 * With no FD argument, ReadSesame becomes a simple
                 * stdin -> stdout stream command.
                 */
                uint8_t buffer[256];

                while (true)
                {
                    const int bytesRead =
                        stream_read(
                            context.stdinFd,
                            buffer,
                            sizeof(buffer)
                        );

                    if (bytesRead <= 0)
                        break;

                    if (!stream_write(
                            context.stdoutFd,
                            buffer,
                            static_cast<uint32_t>(
                                bytesRead
                            )))
                    {
                        break;
                    }
                }

                return true;
            }

            unsigned int fdValue = 0;

            if (!parse_unsigned(
                    arg,
                    fdValue))
            {
                stream_print_string(
                    context.stderrFd,
                    "ReadSesame : invalid file descriptor\n"
                );

                return true;
            }

            const int fd =
                static_cast<int>(fdValue);

            uint8_t buffer[256];

            const int bytesRead =
                filesystem_read(
                    fd,
                    buffer,
                    sizeof(buffer)
                );

            if (bytesRead < 0)
            {
                print_fs_error(
                    context,
                    "ReadSesame"
                );
            }
            else
            {
                stream_write(
                    context.stdoutFd,
                    buffer,
                    static_cast<uint32_t>(
                        bytesRead
                    )
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "SeekSesame",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "SeekSesame : usage: SeekSesame <fd> <offset>\n"
                );

                return true;
            }

            unsigned int fdValue = 0;

            if (!parse_unsigned(
                    arg,
                    fdValue))
            {
                stream_print_string(
                    context.stderrFd,
                    "SeekSesame : invalid file descriptor\n"
                );

                return true;
            }

            unsigned int i = 0;

            while (arg[i] >= '0' &&
                   arg[i] <= '9')
            {
                ++i;
            }

            while (arg[i] == ' ')
                ++i;

            if (arg[i] == '\0')
            {
                stream_print_string(
                    context.stderrFd,
                    "SeekSesame : missing offset\n"
                );

                return true;
            }

            uint64_t offset = 0;

            if (!parse_uint64(
                    &arg[i],
                    offset))
            {
                stream_print_string(
                    context.stderrFd,
                    "SeekSesame : invalid offset\n"
                );

                return true;
            }

            const int fd =
                static_cast<int>(fdValue);

            if (filesystem_seek(
                    fd,
                    offset))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Seeked fd "
                );

                stream_print_number(
                    context.stdoutFd,
                    fdValue
                );

                stream_print_string(
                    context.stdoutFd,
                    " to offset "
                );

                stream_print_number(
                    context.stdoutFd,
                    offset
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "SeekSesame"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "SizeSesame",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "SizeSesame : missing file descriptor\n"
                );

                return true;
            }

            unsigned int fdValue = 0;

            if (!parse_unsigned(
                    arg,
                    fdValue))
            {
                stream_print_string(
                    context.stderrFd,
                    "SizeSesame : invalid file descriptor\n"
                );

                return true;
            }

            uint64_t size = 0;

            if (filesystem_get_file_size(
                    static_cast<int>(fdValue),
                    &size))
            {
                stream_print_string(
                    context.stdoutFd,
                    "File size: "
                );

                stream_print_number(
                    context.stdoutFd,
                    size
                );

                stream_print_string(
                    context.stdoutFd,
                    " bytes\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "SizeSesame"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "WriteSesame",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "WriteSesame : missing arguments\n"
                );

                return true;
            }

            unsigned int fdValue = 0;

            if (!parse_unsigned(
                    arg,
                    fdValue))
            {
                stream_print_string(
                    context.stderrFd,
                    "WriteSesame : invalid file descriptor\n"
                );

                return true;
            }

            unsigned int i = 0;

            while (arg[i] >= '0' &&
                   arg[i] <= '9')
            {
                ++i;
            }

            while (arg[i] == ' ')
                ++i;

            const char* text = &arg[i];

            /*
             * No text means:
             *
             *     WriteSesame <fd>
             *
             * Read stdin and write it into the specified FD.
             */
            if (text[0] == '\0')
            {
                uint8_t buffer[256];

                while (true)
                {
                    const int bytesRead =
                        stream_read(
                            context.stdinFd,
                            buffer,
                            sizeof(buffer)
                        );

                    if (bytesRead <= 0)
                        break;

                    if (!filesystem_write(
                            static_cast<int>(fdValue),
                            buffer,
                            static_cast<uint32_t>(
                                bytesRead
                            )))
                    {
                        print_fs_error(
                            context,
                            "WriteSesame"
                        );

                        break;
                    }
                }

                return true;
            }

            uint32_t textLength = 0;

            while (text[textLength] != '\0')
                ++textLength;

            if (filesystem_write(
                    static_cast<int>(fdValue),
                    reinterpret_cast<const uint8_t*>(
                        text
                    ),
                    textLength))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Wrote "
                );

                stream_print_number(
                    context.stdoutFd,
                    textLength
                );

                stream_print_string(
                    context.stdoutFd,
                    " bytes to fd "
                );

                stream_print_number(
                    context.stdoutFd,
                    fdValue
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "WriteSesame"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "huh?",
                &arg))
        {
            char name[kfs::MAX_NAME_LEN];

            if (!copy_trimmed_argument(
                    arg,
                    name,
                    kfs::MAX_NAME_LEN))
            {
                stream_print_string(
                    context.stderrFd,
                    "huh? : missing name\n"
                );
            }
            else if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (name[0] == '.' &&
                     name[1] == '.' &&
                     name[2] == '\0')
            {
                if (filesystem_change_directory(name))
                {
                    stream_print_string(
                        context.stdoutFd,
                        "heading back!\n"
                    );
                }
                else
                {
                    stream_print_string(
                        context.stderrFd,
                        "hmmm... nowhere to head back to\n"
                    );
                }
            }
            else if (filesystem_change_directory(name))
            {
                stream_print_string(
                    context.stdoutFd,
                    "I found something called "
                );

                stream_print_string(
                    context.stdoutFd,
                    name
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                stream_print_string(
                    context.stderrFd,
                    "hmmm... i cant find it\n"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "WhyIsItEchoing?@Here",
                &arg))
        {
            if (arg == nullptr)
            {
                stream_print_string(
                    context.stderrFd,
                    "WhyIsItEchoing?@Here : missing arguments\n"
                );

                return true;
            }

            char name[kfs::MAX_NAME_LEN];

            unsigned int i = 0;

            while (arg[i] != ' ' &&
                   arg[i] != '\0' &&
                   i < kfs::MAX_NAME_LEN - 1)
            {
                name[i] = arg[i];
                ++i;
            }

            name[i] = '\0';

            while (arg[i] == ' ')
                ++i;

            const char* text = &arg[i];

            uint32_t textLength = 0;

            while (text[textLength] != '\0')
                ++textLength;

            if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS mount failed\n"
                );
            }
            else if (filesystem_write_file(
                         name,
                         reinterpret_cast<const uint8_t*>(
                             text
                         ),
                         textLength))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Wrote to "
                );

                stream_print_string(
                    context.stdoutFd,
                    name
                );

                stream_print_string(
                    context.stdoutFd,
                    "\n"
                );
            }
            else
            {
                print_fs_error(
                    context,
                    "WhyIsItEchoing?@Here"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "UnmountTheEagle!@Filesystem",
                &arg))
        {
            if (!filesystem_is_mounted())
            {
                stream_print_string(
                    context.stdoutFd,
                    "TalonsFS is already unmounted\n"
                );
            }
            else if (filesystem_unmount())
            {
                stream_print_string(
                    context.stdoutFd,
                    "TalonsFS unmounted successfully!\n"
                );
            }
            else
            {
                stream_print_string(
                    context.stderrFd,
                    "TalonsFS unmount failed!\n"
                );
            }

            return true;
        }

        if (command_matches(
                line,
                "Byebye!",
                &arg))
        {
            /*
             * The BlockCache is write-back, so unmount before reboot.
             */
            if (filesystem_is_mounted())
            {
                if (!filesystem_unmount())
                {
                    stream_print_string(
                        context.stderrFd,
                        "WARNING: unmount failed; cached data may be lost\n"
                    );
                }
            }

            stream_print_string(
                context.stdoutFd,
                "Rebooting...\n"
            );

            /*
             * The reboot implementation lives below. This command is
             * intentionally terminal-oriented and should not normally
             * be used inside a pipeline.
             */
            for (uint32_t i = 0;
                 i < 100000;
                 ++i)
            {
                asm volatile ("pause");
            }

            asm volatile ("cli");

            struct [[gnu::packed]]
            {
                uint16_t limit;
                uint64_t base;
            } nullIdt = { 0, 0 };

            asm volatile (
                "lidt %0"
                :
                : "m"(nullIdt)
            );

            asm volatile ("int $0x03");

            for (;;)
            {
                asm volatile ("hlt");
            }
        }

        if (command_matches(
                line,
                "FormatTheEagle!@Disk",
                &arg))
        {
            stream_print_string(
                context.stdoutFd,
                "Format the entire TalonsFS filesystem? [y/n]\n"
            );

            char confirmation[8];

            if (!copy_trimmed_argument(
                    arg,
                    confirmation,
                    sizeof(confirmation)))
            {
                stream_print_string(
                    context.stdoutFd,
                    "Formatting cancelled.\n"
                );
            }
            else if (confirmation[0] == 'y' &&
                     confirmation[1] == '\0')
            {
                stream_print_string(
                    context.stdoutFd,
                    "Formatting TalonsFS...\n"
                );

                if (filesystem_format())
                {
                    stream_print_string(
                        context.stdoutFd,
                        "TalonsFS formatted successfully!\n"
                    );
                }
                else
                {
                    stream_print_string(
                        context.stderrFd,
                        "TalonsFS formatting failed!\n"
                    );
                }
            }
            else
            {
                stream_print_string(
                    context.stdoutFd,
                    "Formatting cancelled.\n"
                );
            }

            return true;
        }

        stream_print_string(
            context.stderrFd,
            "Thats not a command buddy!\n"
        );

        return false;
    }

    /*
     * 8042 reset fallback.
     *
     * Kept separate from the command dispatcher so the shell architecture
     * itself does not depend on the reboot mechanism.
     */

     static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    asm volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

     [[noreturn]] static void reboot_machine()
{
    /*
     * Reset the machine through the 8042 keyboard controller.
     *
     * Wait for the controller input buffer to become empty before
     * sending the reset command. The bounded loop prevents a broken
     * controller from hanging the kernel forever.
     */
    for (uint32_t i = 0; i < 100000; ++i)
    {
        if ((inb(0x64) & 0x02) == 0)
        {
            break;
        }
    }

    outb(0x64, 0xFE);

    /*
     * Give the controller a moment to assert reset.
     */
    for (uint32_t i = 0; i < 1000000; ++i)
    {
        asm volatile ("pause");
    }

    /*
     * Fallback: deliberate triple fault.
     *
     * A zero-length IDT means the interrupt cannot be dispatched.
     * The resulting fault causes another fault, eventually producing
     * a CPU reset.
     */
    struct [[gnu::packed]]
    {
        uint16_t limit;
        uint64_t base;
    } nullIdt = { 0, 0 };

    asm volatile ("lidt %0" : : "m"(nullIdt));
    asm volatile ("int $0x03");

    for (;;)
    {
        asm volatile ("hlt");
    }
}

}

extern "C" void shell_handle_line(
    const char* line)
{
    if (line == nullptr)
        return;

    char workingLine[SHELL_LINE_MAX];

    copy_string(
        line,
        workingLine,
        sizeof(workingLine)
    );

    trim_spaces(workingLine);

    if (workingLine[0] == '\0')
        return;

    char commandParts[
        MAX_COMMANDS
    ][SHELL_LINE_MAX];

    unsigned int commandCount = 0;

    if (!split_pipeline(
            workingLine,
            commandParts,
            commandCount))
    {
        stream_write_string(
            STDERR_FILENO,
            "Shell: invalid pipeline\n"
        );

        return;
    }

    ParsedCommand commands[
        MAX_COMMANDS
    ];

    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        if (!parse_command(
                commandParts[i],
                commands[i]))
        {
            stream_write_string(
                STDERR_FILENO,
                "Shell: invalid command syntax\n"
            );

            return;
        }
    }

    /*
     * A redirection on the same stream as a pipe would be ambiguous in
     * this first shell implementation. Reject it instead of silently
     * doing the wrong thing.
     */
    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        if (i > 0 &&
            commands[i].hasInputRedirect)
        {
            stream_write_string(
                STDERR_FILENO,
                "Shell: input redirection conflicts with a pipe\n"
            );

            return;
        }

        if (i + 1 < commandCount &&
            commands[i].hasOutputRedirect)
        {
            stream_write_string(
                STDERR_FILENO,
                "Shell: output redirection conflicts with a pipe\n"
            );

            return;
        }
    }

    CommandContext contexts[
        MAX_COMMANDS
    ];

    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        contexts[i].stdinFd =
            STDIN_FILENO;

        contexts[i].stdoutFd =
            STDOUT_FILENO;

        contexts[i].stderrFd =
            STDERR_FILENO;
    }

    int pipeReadFds[
        MAX_COMMANDS - 1
    ];

    int pipeWriteFds[
        MAX_COMMANDS - 1
    ];

    for (unsigned int i = 0;
         i + 1 < commandCount;
         ++i)
    {
        pipeReadFds[i] = -1;
        pipeWriteFds[i] = -1;

        if (!filesystem_create_pipe(
                pipeReadFds[i],
                pipeWriteFds[i]))
        {
            stream_write_string(
                STDERR_FILENO,
                "Shell: failed to create pipe\n"
            );

            for (unsigned int j = 0;
                 j < i;
                 ++j)
            {
                if (pipeReadFds[j] >= 0)
                    filesystem_close(
                        pipeReadFds[j]
                    );

                if (pipeWriteFds[j] >= 0)
                    filesystem_close(
                        pipeWriteFds[j]
                    );
            }

            return;
        }

        contexts[i].stdoutFd =
            pipeWriteFds[i];

        contexts[i + 1].stdinFd =
            pipeReadFds[i];
    }

    /*
     * Apply file redirections after the pipeline endpoints have been
     * assigned. Conflicts were already rejected above.
     */
    int redirectedInputFds[
        MAX_COMMANDS
    ];

    int redirectedOutputFds[
        MAX_COMMANDS
    ];

    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        redirectedInputFds[i] = -1;
        redirectedOutputFds[i] = -1;
    }

    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        if (commands[i].hasInputRedirect)
        {
            redirectedInputFds[i] =
                open_input_redirect(
                    commands[i].inputFile
                );

            if (redirectedInputFds[i] < 0)
            {
                stream_write_string(
                    STDERR_FILENO,
                    "Shell: failed to open input file\n"
                );

                for (unsigned int j = 0;
                     j < commandCount;
                     ++j)
                {
                    if (redirectedInputFds[j] >= 0)
                        filesystem_close(
                            redirectedInputFds[j]
                        );

                    if (redirectedOutputFds[j] >= 0)
                        filesystem_close(
                            redirectedOutputFds[j]
                        );
                }

                for (unsigned int j = 0;
                     j + 1 < commandCount;
                     ++j)
                {
                    if (pipeReadFds[j] >= 0)
                        filesystem_close(
                            pipeReadFds[j]
                        );

                    if (pipeWriteFds[j] >= 0)
                        filesystem_close(
                            pipeWriteFds[j]
                        );
                }

                return;
            }

            contexts[i].stdinFd =
                redirectedInputFds[i];
        }

        if (commands[i].hasOutputRedirect)
        {
            redirectedOutputFds[i] =
                open_output_redirect(
                    commands[i].outputFile
                );

            if (redirectedOutputFds[i] < 0)
            {
                stream_write_string(
                    STDERR_FILENO,
                    "Shell: failed to open output file\n"
                );

                for (unsigned int j = 0;
                     j < commandCount;
                     ++j)
                {
                    if (redirectedInputFds[j] >= 0)
                        filesystem_close(
                            redirectedInputFds[j]
                        );

                    if (redirectedOutputFds[j] >= 0)
                        filesystem_close(
                            redirectedOutputFds[j]
                        );
                }

                for (unsigned int j = 0;
                     j + 1 < commandCount;
                     ++j)
                {
                    if (pipeReadFds[j] >= 0)
                        filesystem_close(
                            pipeReadFds[j]
                        );

                    if (pipeWriteFds[j] >= 0)
                        filesystem_close(
                            pipeWriteFds[j]
                        );
                }

                return;
            }

            contexts[i].stdoutFd =
                redirectedOutputFds[i];
        }
    }

    /*
     * Execute each stage from left to right.
     *
     * This is intentionally sequential for now. The pipe implementation
     * currently has a bounded 4096-byte buffer and no blocking scheduler,
     * so sequential execution avoids pretending we have a process model
     * that TalonsOS does not yet possess.
     */
    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        execute_command(
            commands[i].command,
            contexts[i]
        );

        /*
         * Once a command has finished, its output pipe write end can be
         * closed. The next command then sees the completed pipe contents.
         */
        if (i + 1 < commandCount &&
            pipeWriteFds[i] >= 0)
        {
            filesystem_close(
                pipeWriteFds[i]
            );

            pipeWriteFds[i] = -1;
        }
    }

    /*
     * Close all temporary shell-owned descriptors.
     */
    for (unsigned int i = 0;
         i < commandCount;
         ++i)
    {
        if (redirectedInputFds[i] >= 0)
        {
            filesystem_close(
                redirectedInputFds[i]
            );

            redirectedInputFds[i] = -1;
        }

        if (redirectedOutputFds[i] >= 0)
        {
            filesystem_close(
                redirectedOutputFds[i]
            );

            redirectedOutputFds[i] = -1;
        }
    }

    for (unsigned int i = 0;
         i + 1 < commandCount;
         ++i)
    {
        if (pipeReadFds[i] >= 0)
        {
            filesystem_close(
                pipeReadFds[i]
            );

            pipeReadFds[i] = -1;
        }

        if (pipeWriteFds[i] >= 0)
        {
            filesystem_close(
                pipeWriteFds[i]
            );

            pipeWriteFds[i] = -1;
        }
    }
}
