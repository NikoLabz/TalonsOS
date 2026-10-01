#include "standard_streams.h"
#include "terminal.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"

namespace
{
    const unsigned char colour = 0x07;

    const char* prompt = "Developer@TalonsOS>";
    const unsigned short prompt_length = 18;

    unsigned short cursor_position = 80 + prompt_length;

    char command_buffer[80];
    unsigned short command_length = 0;

    // Position of the cursor inside command_buffer.
    unsigned short command_cursor_position = 0;

    const unsigned short HISTORY_SIZE = 16;

    char command_history[HISTORY_SIZE][80];
    unsigned short history_count = 0;
    unsigned short history_position = 0;

    char saved_command[80];

    void print_prompt()
    {
        int i = 0;

        while (prompt[i] != '\0')
        {
            vga_putchar_at(
                cursor_position,
                prompt[i],
                colour
            );

            ++cursor_position;
            ++i;
        }
    }

    void clear_current_command()
    {
        unsigned short position =
            cursor_position - command_cursor_position;

        for (unsigned short i = 0; i < command_length; ++i)
        {
            vga_putchar_at(
                position + i,
                ' ',
                colour
            );
        }

        cursor_position = position;

        vga_update_cursor(cursor_position);
    }

    void load_history_command(unsigned short index)
    {
        clear_current_command();

        unsigned short length = 0;

        while (command_history[index][length] != '\0' &&
               length < 79)
        {
            command_buffer[length] =
                command_history[index][length];

            ++length;
        }

        command_length = length;
        command_cursor_position = command_length;

        command_buffer[command_length] =
            '\0';

        for (unsigned short i = 0; i < command_length; ++i)
        {
            vga_putchar_at(
                cursor_position,
                command_buffer[i],
                colour
            );

            ++cursor_position;
        }

        vga_update_cursor(cursor_position);
    }

    void save_command_to_history()
    {
        if (command_length == 0)
        {
            return;
        }

        if (history_count > 0)
        {
            bool duplicate = true;

            for (unsigned short i = 0; i < command_length; ++i)
            {
                if (command_history[history_count - 1][i] !=
                    command_buffer[i])
                {
                    duplicate = false;
                    break;
                }
            }

            if (duplicate &&
                command_history[history_count - 1][command_length] == '\0')
            {
                return;
            }
        }

        if (history_count >= HISTORY_SIZE)
        {
            for (unsigned short i = 1; i < HISTORY_SIZE; ++i)
            {
                for (unsigned short j = 0; j < 80; ++j)
                {
                    command_history[i - 1][j] =
                        command_history[i][j];
                }
            }

            --history_count;
        }

        for (unsigned short i = 0; i < command_length; ++i)
        {
            command_history[history_count][i] =
                command_buffer[i];
        }

        command_history[history_count][command_length] =
            '\0';

        ++history_count;
    }

    void move_to_next_row()
    {
        cursor_position =
            ((cursor_position / 80) + 1) * 80;

        if (cursor_position >= 2000)
        {
            vga_scroll(colour);

            cursor_position = 1920;
        }
    }
}

void terminal_init()
{
    cursor_position = 160;

    print_prompt();

    vga_update_cursor(
        cursor_position
    );
}

unsigned short terminal_get_output_position()
{
    return cursor_position;
}

void terminal_set_output_position(unsigned short position)
{
    while (position >= 2000)
    {
        vga_scroll(colour);
        position -= 80;
    }

    cursor_position = position;

    vga_update_cursor(cursor_position);
}

void terminal_run()
{
    while (true)
    {

	char character =
    static_cast<char>(stdin_read_char());

        if (character == 0)
        {
            continue;
        }

        // ========================================================
        // HISTORY: UP
        // ========================================================

        if (character == KEY_ARROW_UP)
        {
            if (history_count > 0)
            {
                if (history_position == history_count)
                {
                    for (unsigned short i = 0;
                         i < command_length;
                         ++i)
                    {
                        saved_command[i] =
                            command_buffer[i];
                    }

                    saved_command[command_length] =
                        '\0';
                }

                if (history_position > 0)
                {
                    --history_position;
                }

                load_history_command(history_position);
            }

            continue;
        }

        // ========================================================
        // HISTORY: DOWN
        // ========================================================

        else if (character == KEY_ARROW_DOWN)
        {
            if (history_count > 0)
            {
                if (history_position < history_count)
                {
                    ++history_position;
                }

                if (history_position < history_count)
                {
                    load_history_command(history_position);
                }
                else
                {
                    clear_current_command();

                    unsigned short length = 0;

                    while (saved_command[length] != '\0' &&
                           length < 79)
                    {
                        command_buffer[length] =
                            saved_command[length];

                        ++length;
                    }

                    command_length = length;
                    command_cursor_position = command_length;

                    command_buffer[command_length] =
                        '\0';

                    for (unsigned short i = 0;
                         i < command_length;
                         ++i)
                    {
                        vga_putchar_at(
                            cursor_position,
                            command_buffer[i],
                            colour
                        );

                        ++cursor_position;
                    }

                    vga_update_cursor(cursor_position);
                }

                continue;
            }
        }

        // ========================================================
        // CURSOR: LEFT
        // ========================================================

        else if (character == KEY_ARROW_LEFT)
        {
            if (command_cursor_position > 0)
            {
                --command_cursor_position;
                --cursor_position;

                vga_update_cursor(cursor_position);
            }

            continue;
        }

        // ========================================================
        // CURSOR: RIGHT
        // ========================================================

        else if (character == KEY_ARROW_RIGHT)
        {
            if (command_cursor_position < command_length)
            {
                ++command_cursor_position;
                ++cursor_position;

                vga_update_cursor(cursor_position);
            }

            continue;
        }

        // ========================================================
        // BACKSPACE
        // ========================================================

        if (character == '\b')
        {
            if (command_cursor_position > 0)
            {
                --command_cursor_position;
                --command_length;
                --cursor_position;

                command_buffer[command_length] =
                    '\0';

                vga_putchar_at(
                    cursor_position,
                    ' ',
                    colour
                );

                vga_update_cursor(
                    cursor_position
                );
            }
        }

        // ========================================================
        // ENTER
        // ========================================================

        else if (character == '\n')
        {
            command_buffer[command_length] =
                '\0';

            save_command_to_history();

            history_position = history_count;

            move_to_next_row();

            shell_handle_line(
                command_buffer
            );

            move_to_next_row();

            print_prompt();

            command_length = 0;
            command_cursor_position = 0;

            command_buffer[0] =
                '\0';

            vga_update_cursor(
                cursor_position
            );
        }

        // ========================================================
        // NORMAL CHARACTER
        // ========================================================

        else
        {
            if (command_length < 79)
            {
                command_buffer[command_length] =
                    character;

                ++command_length;
                ++command_cursor_position;

                command_buffer[command_length] =
                    '\0';

                vga_putchar_at(
                    cursor_position,
                    character,
                    colour
                );

                ++cursor_position;

                if (cursor_position >= 2000)
                {
                    vga_scroll(colour);

                    cursor_position = 1920;
                }

                vga_update_cursor(
                    cursor_position
                );
            }
        }
    }
}                                                              
