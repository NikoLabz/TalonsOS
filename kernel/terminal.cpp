#include "terminal.h"
#include "vga.h"
#include "keyboard.h"
#include "shell.h"

namespace
{
    const unsigned char colour = 0x07;

    // ============================================================
    // PROMPT
    // ============================================================

    const char* prompt = "Developer@TalonsOS>";

    const unsigned short prompt_length = 18;

    // Current VGA text-cell cursor position.
    unsigned short cursor_position = 80 + prompt_length;

    // ============================================================
    // COMMAND BUFFER
    // ============================================================

    char command_buffer[80];
    unsigned short command_length = 0;

    const unsigned short HISTORY_SIZE = 16;

    char command_history[HISTORY_SIZE][80];
    unsigned short history_count = 0;
    unsigned short history_position = 0;

    char saved_command[80];

 
    // ============================================================
// PRINT PROMPT
// ============================================================

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


// ============================================================
// CLEAR CURRENT COMMAND
// ============================================================

void clear_current_command()
{
    unsigned short position =
        cursor_position - command_length;

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

    // Do not store consecutive duplicate commands.
    if (history_count > 0)
    {
        bool duplicate = true;

        for (unsigned short i = 0; i < command_length; ++i)
        {
            if (command_history[history_count - 1][i] != command_buffer[i])
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

    // If history is full, discard the oldest command.
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

    // ============================================================
    // MOVE TO NEXT ROW
    // ============================================================

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


// ============================================================
// TERMINAL INITIALISATION
// ============================================================

void terminal_init()
{
    cursor_position = 160;

    print_prompt();

    vga_update_cursor(
        cursor_position
    );
}


// ============================================================
// GET CURRENT OUTPUT POSITION
// ============================================================

unsigned short terminal_get_output_position()
{
    return cursor_position;
}

void terminal_set_output_position(unsigned short position)
{
    // A single print_string() call can contain more than one '\n'
    // (e.g. shell.cpp's directory-listing banner), which can advance
    // `position` past the bottom of the screen by MORE than one row
    // in a single call. A plain `if` only ever scrolls once and
    // subtracts one row (80 cells), which is not enough to bring a
    // multi-row overshoot back into the visible 80x25 window (cells
    // 0..1999). Left uncorrected, `cursor_position` is stored >= 2000
    // and every character written at that position lands off-screen
    // (invisible, and never reachable by vga_scroll, which only ever
    // moves cells 0..1999) until enough LATER calls each shave off
    // another 80 to finally bring it back under 2000 -- so trailing
    // output (e.g. the final "N objects." line) can reappear while
    // earlier output written during the overshoot (e.g. a directory
    // entry's label and name) is silently lost.
    //
    // A `while` loop scrolls exactly as many times as needed and
    // fully normalizes `position` before it is ever stored or used
    // as a write target, regardless of how many rows were skipped in
    // one call. This mirrors the (already-correct) fixed-row-reset
    // used by move_to_next_row() below, generalized to handle an
    // overshoot of more than one row.
    while (position >= 2000)
    {
        vga_scroll(colour);
        position -= 80;
    }

    cursor_position = position;

    vga_update_cursor(cursor_position);
}

// ============================================================
// TERMINAL LOOP
// ============================================================

void terminal_run()
{
    while (true)
    {
        char character =
            keyboard_poll_char();

        if (character == 0)
        {
            continue;
        }
        
// ========================================================
// ARROW UP
// ========================================================

if (character == KEY_ARROW_UP)
{
    if (history_count > 0)
    {
        if (history_position == history_count)
        {
            for (unsigned short i = 0; i < command_length; ++i)
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
// ARROW DOWN
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
     
	continue;
    }
}

        // ========================================================
        // BACKSPACE
        // ========================================================

        if (character == '\b')
        {
            if (command_length > 0)
            {
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

            // Move below the command.

            move_to_next_row();


            // ====================================================
            // SHELL
            // ====================================================

            shell_handle_line(
                command_buffer
            );


            // Move below shell output.

            move_to_next_row();


            // ====================================================
            // NEW PROMPT
            // ====================================================

            print_prompt();


            // ====================================================
            // CLEAR COMMAND BUFFER
            // ====================================================

            command_length = 0;

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

                command_buffer[command_length] =
                    '\0';


                vga_putchar_at(
                    cursor_position,
                    character,
                    colour
                );

                ++cursor_position;


                // If we reach the bottom of the screen,
                // scroll upward.

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
