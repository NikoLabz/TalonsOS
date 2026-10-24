#include "editor.h"
#include "filesystem.h"
#include "keyboard.h"
#include "terminal.h"
#include "vga.h"

namespace
{
    constexpr unsigned int SCREEN_WIDTH = 80;
    constexpr unsigned int SCREEN_HEIGHT = 25;
    constexpr unsigned int EDIT_ROWS = 23;
    constexpr unsigned int MAX_TEXT = 4095;
    constexpr unsigned char COLOUR = 0x07;
    constexpr unsigned char STATUS_COLOUR = 0x70;

    char text[MAX_TEXT + 1];
    unsigned int length = 0;
    unsigned int cursor = 0;
    unsigned int scroll_row = 0;
    bool insert_mode = false;
    bool dirty = false;

    void clear_screen()
    {
        for (unsigned int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
            vga_putchar_at(i, ' ', COLOUR);
    }

    void calculate_position(
        unsigned int index,
        unsigned int& row,
        unsigned int& column)
    {
        row = 0;
        column = 0;

        for (unsigned int i = 0; i < index && i < length; ++i)
        {
            if (text[i] == '\n')
            {
                ++row;
                column = 0;
            }
            else
            {
                ++column;
                if (column >= SCREEN_WIDTH)
                {
                    ++row;
                    column = 0;
                }
            }
        }
    }

    void redraw(const char* filename, const char* message)
    {
        clear_screen();

        unsigned int row = 0;
        unsigned int column = 0;
        unsigned int visible_row = 0;

        for (unsigned int i = 0; i < length; ++i)
        {
            if (text[i] == '\n')
            {
                ++row;
                column = 0;
                continue;
            }

            if (row >= scroll_row && row < scroll_row + EDIT_ROWS)
            {
                const unsigned int pos =
                    (row - scroll_row) * SCREEN_WIDTH + column;

                if (pos < EDIT_ROWS * SCREEN_WIDTH)
                    vga_putchar_at(pos, text[i], COLOUR);
            }

            ++column;
            if (column >= SCREEN_WIDTH)
            {
                ++row;
                column = 0;
            }
        }

        for (unsigned int i = 0; i < SCREEN_WIDTH; ++i)
            vga_putchar_at(EDIT_ROWS * SCREEN_WIDTH + i, ' ', STATUS_COLOUR);

        const char* mode = insert_mode ? "-- INSERT -- " : "-- NORMAL -- ";
        unsigned int p = EDIT_ROWS * SCREEN_WIDTH;

        for (unsigned int i = 0; mode[i] && p < (EDIT_ROWS + 1) * SCREEN_WIDTH; ++i)
            vga_putchar_at(p++, mode[i], STATUS_COLOUR);

        if (dirty)
            vga_putchar_at(p++, '*', STATUS_COLOUR);

        for (unsigned int i = 0; filename[i] && p < (EDIT_ROWS + 1) * SCREEN_WIDTH; ++i)
            vga_putchar_at(p++, filename[i], STATUS_COLOUR);

        for (unsigned int i = 0; message[i] && p < (EDIT_ROWS + 1) * SCREEN_WIDTH; ++i)
            vga_putchar_at(p++, message[i], STATUS_COLOUR);

        for (unsigned int i = 0; i < SCREEN_WIDTH; ++i)
            vga_putchar_at((EDIT_ROWS + 1) * SCREEN_WIDTH + i, ' ', COLOUR);

        unsigned int cr = 0;
        unsigned int cc = 0;
        calculate_position(cursor, cr, cc);

        if (cr < scroll_row)
            scroll_row = cr;
        else if (cr >= scroll_row + EDIT_ROWS)
            scroll_row = cr - EDIT_ROWS + 1;

        // Redraw again if the cursor moved the viewport.
        if (cr < scroll_row || cr >= scroll_row + EDIT_ROWS)
            return;

        const unsigned int screen_pos =
            (cr - scroll_row) * SCREEN_WIDTH +
            (cc < SCREEN_WIDTH ? cc : SCREEN_WIDTH - 1);

        vga_update_cursor(static_cast<unsigned short>(screen_pos));
        (void)visible_row;
    }

    bool split_path(
        const char* path,
        char* directory,
        unsigned int directory_capacity,
        char* filename,
        unsigned int filename_capacity)
    {
        if (path == nullptr || path[0] == '\0')
            return false;

        unsigned int length = 0;
        while (path[length] != '\0')
            ++length;

        unsigned int slash = length;
        while (slash > 0 && path[slash - 1] != '/')
            --slash;

        unsigned int name_length = length - slash;
        if (name_length == 0 || name_length >= filename_capacity)
            return false;

        for (unsigned int i = 0; i < name_length; ++i)
            filename[i] = path[slash + i];
        filename[name_length] = '\0';

        if (slash == 0)
        {
            directory[0] = '.';
            directory[1] = '\0';
            return true;
        }

        unsigned int dir_length = slash - 1;
        if (dir_length == 0)
        {
            if (directory_capacity < 2)
                return false;
            directory[0] = '/';
            directory[1] = '\0';
            return true;
        }

        if (dir_length >= directory_capacity)
            return false;

        for (unsigned int i = 0; i < dir_length; ++i)
            directory[i] = path[i];
        directory[dir_length] = '\0';
        return true;
    }

    bool go_to_root()
    {
        for (unsigned int i = 0; i < 64; ++i)
        {
            const char* current = filesystem_current_directory_path();
            if (current == nullptr)
                return false;
            if (current[0] == '/' && current[1] == '\0')
                return true;
            if (!filesystem_change_directory(".."))
                return false;
        }
        return false;
    }

    bool enter_file_directory(
        const char* path,
        char* original_directory,
        unsigned int original_capacity,
        char* base_name,
        unsigned int base_capacity)
    {
        const char* current = filesystem_current_directory_path();
        if (current == nullptr)
            return false;

        unsigned int i = 0;
        while (current[i] != '\0' && i + 1 < original_capacity)
        {
            original_directory[i] = current[i];
            ++i;
        }
        if (current[i] != '\0')
            return false;
        original_directory[i] = '\0';

        char directory[128];
        if (!split_path(path, directory, sizeof(directory),
                        base_name, base_capacity))
            return false;

        if (directory[0] == '.')
            return true;

        unsigned int start = 0;
        if (directory[0] == '/')
        {
            if (!go_to_root())
                return false;
            start = 1;
        }

        while (directory[start] != '\0')
        {
            char component[64];
            unsigned int end = start;
            while (directory[end] != '\0' && directory[end] != '/')
                ++end;

            unsigned int n = end - start;
            if (n == 0 || n >= sizeof(component))
                return false;

            for (unsigned int j = 0; j < n; ++j)
                component[j] = directory[start + j];
            component[n] = '\0';

            if (!filesystem_change_directory(component))
                return false;

            start = end;
            if (directory[start] == '/')
                ++start;
        }

        return true;
    }

    void restore_directory(const char* original)
    {
        if (original == nullptr || original[0] == '\0')
            return;

        if (!go_to_root())
            return;

        unsigned int start = 1;
        while (original[start] != '\0')
        {
            char component[64];
            unsigned int end = start;
            while (original[end] != '\0' && original[end] != '/')
                ++end;

            unsigned int n = end - start;
            if (n == 0 || n >= sizeof(component))
                return;

            for (unsigned int j = 0; j < n; ++j)
                component[j] = original[start + j];
            component[n] = '\0';

            if (!filesystem_change_directory(component))
                return;

            start = end;
            if (original[start] == '/')
                ++start;
        }
    }

    bool save_file(const char* path)
    {
        char original_directory[128];
        char base_name[64];

        if (!enter_file_directory(
                path, original_directory, sizeof(original_directory),
                base_name, sizeof(base_name)))
        {
            restore_directory(original_directory);
            return false;
        }

        bool ok = filesystem_write_file(
            base_name,
            reinterpret_cast<const uint8_t*>(text),
            length
        );

        if (!ok && filesystem_get_last_error() == FilesystemError::NotFound)
        {
            if (filesystem_create_file(base_name))
            {
                ok = filesystem_write_file(
                    base_name,
                    reinterpret_cast<const uint8_t*>(text),
                    length
                );
            }
        }

        restore_directory(original_directory);

        if (ok)
            dirty = false;

        return ok;
    }

    void insert_character(char c)
    {
        if (length >= MAX_TEXT)
            return;

        for (unsigned int i = length; i > cursor; --i)
            text[i] = text[i - 1];

        text[cursor++] = c;
        ++length;
        text[length] = '\0';
        dirty = true;
    }

    void delete_before_cursor()
    {
        if (cursor == 0)
            return;

        for (unsigned int i = cursor - 1; i + 1 < length; ++i)
            text[i] = text[i + 1];

        --cursor;
        --length;
        text[length] = '\0';
        dirty = true;
    }

    void move_left()
    {
        if (cursor > 0)
            --cursor;
    }

    void move_right()
    {
        if (cursor < length)
            ++cursor;
    }

    void move_vertical(bool down)
    {
        unsigned int row = 0;
        unsigned int column = 0;
        calculate_position(cursor, row, column);

        if (down)
        {
            if (cursor >= length)
                return;

            unsigned int i = cursor;
            while (i < length && text[i] != '\n')
                ++i;

            if (i == length)
                return;

            ++i;
            for (unsigned int c = 0; c < column && i < length && text[i] != '\n'; ++c)
                ++i;

            cursor = i;
        }
        else
        {
            if (row == 0)
                return;

            unsigned int line_start = cursor;
            while (line_start > 0 && text[line_start - 1] != '\n')
                --line_start;

            if (line_start == 0)
                return;

            unsigned int previous_end = line_start - 1;
            unsigned int previous_start = previous_end;

            while (previous_start > 0 && text[previous_start - 1] != '\n')
                --previous_start;

            cursor = previous_start;
            for (unsigned int c = 0; c < column && cursor < previous_end; ++c)
                ++cursor;
        }
    }

    bool run_command(
        const char* command,
        const char* filename,
        bool& quit,
        char* message)
    {
        quit = false;

        if (command[0] == 'w' && command[1] == '\0')
        {
            if (save_file(filename))
            {
                message[0] = 'S'; message[1] = 'a'; message[2] = 'v';
                message[3] = 'e'; message[4] = 'd'; message[5] = '\0';
            }
            else
            {
                message[0] = 'S'; message[1] = 'a'; message[2] = 'v';
                message[3] = 'e'; message[4] = ' '; message[5] = 'f';
                message[6] = 'a'; message[7] = 'i'; message[8] = 'l';
                message[9] = 'e'; message[10] = 'd'; message[11] = '\0';
            }
            return true;
        }

        if (command[0] == 'w' && command[1] == 'q' && command[2] == '\0')
        {
            if (save_file(filename))
                quit = true;
            else
            {
                const char error[] = "Save failed";
                unsigned int i = 0;
                while (error[i]) { message[i] = error[i]; ++i; }
                message[i] = '\0';
            }
            return true;
        }

        if (command[0] == 'q' && command[1] == '\0')
        {
            if (!dirty)
                quit = true;
            else
            {
                const char warning[] = "Unsaved changes; use :q!";
                unsigned int i = 0;
                while (warning[i]) { message[i] = warning[i]; ++i; }
                message[i] = '\0';
            }
            return true;
        }

        if (command[0] == 'q' && command[1] == '!' && command[2] == '\0')
        {
            quit = true;
            return true;
        }

        const char invalid[] = "Unknown command";
        unsigned int i = 0;
        while (invalid[i]) { message[i] = invalid[i]; ++i; }
        message[i] = '\0';
        return false;
    }
}

void talonedit_run(const char* filename)
{
    if (filename == nullptr || filename[0] == '\0')
        return;

    length = 0;
    cursor = 0;
    scroll_row = 0;
    insert_mode = false;
    dirty = false;
    text[0] = '\0';

    uint64_t file_size = 0;
    char original_directory[128];
    char base_name[64];

    if (!enter_file_directory(
            filename, original_directory, sizeof(original_directory),
            base_name, sizeof(base_name)))
    {
        restore_directory(original_directory);
        return;
    }

    bool loaded = filesystem_read_file(
        base_name,
        reinterpret_cast<uint8_t*>(text),
        MAX_TEXT,
        &file_size
    );

    FilesystemError load_error = filesystem_get_last_error();
    restore_directory(original_directory);

    if (loaded)
    {
        if (file_size > MAX_TEXT)
            return;

        length = static_cast<unsigned int>(file_size);
        text[length] = '\0';
    }
    else if (load_error != FilesystemError::NotFound)
    {
        return;
    }

    unsigned short screen_snapshot[2000];
    vga_save_screen(screen_snapshot);

    const unsigned short old_position = terminal_get_output_position();
    char message[64] = "Press i to insert; : for commands";
    bool command_mode = false;
    char command[16];
    unsigned int command_length = 0;

    bool quit = false;

    while (!quit)
    {
        redraw(filename, message);

        if (command_mode)
        {
            for (unsigned int i = 0; i < SCREEN_WIDTH; ++i)
                vga_putchar_at((EDIT_ROWS + 2) * SCREEN_WIDTH + i, ' ', COLOUR);

            vga_putchar_at((EDIT_ROWS + 2) * SCREEN_WIDTH, ':', COLOUR);
            for (unsigned int i = 0; i < command_length; ++i)
                vga_putchar_at((EDIT_ROWS + 2) * SCREEN_WIDTH + 1 + i, command[i], COLOUR);

            vga_update_cursor(static_cast<unsigned short>(
                (EDIT_ROWS + 2) * SCREEN_WIDTH + 1 + command_length));

            char key = 0;
            while (key == 0)
                key = keyboard_poll_char();

            if (key == 27)
            {
                command_mode = false;
                command_length = 0;
            }
            else if (key == '\n')
            {
                command[command_length] = '\0';
                run_command(command, filename, quit, message);
                command_mode = false;
                command_length = 0;
            }
            else if (key == '\b')
            {
                if (command_length > 0)
                    --command_length;
            }
            else if (key >= 32 && key <= 126 && command_length < sizeof(command) - 1)
            {
                command[command_length++] = key;
            }
            continue;
        }

        char key = 0;
        while (key == 0)
            key = keyboard_poll_char();

        if (insert_mode)
        {
            if (key == 27)
                insert_mode = false;
            else if (key == '\b')
                delete_before_cursor();
            else if (key == '\n')
                insert_character('\n');
            else if (key == KEY_ARROW_LEFT)
                move_left();
            else if (key == KEY_ARROW_RIGHT)
                move_right();
            else if (key == KEY_ARROW_UP)
                move_vertical(false);
            else if (key == KEY_ARROW_DOWN)
                move_vertical(true);
            else if (key >= 32 && key <= 126)
                insert_character(key);
        }
        else
        {
            if (key == 'i')
                insert_mode = true;
            else if (key == ':')
            {
                command_mode = true;
                command_length = 0;
            }
            else if (key == 'h' || key == KEY_ARROW_LEFT)
                move_left();
            else if (key == 'l' || key == KEY_ARROW_RIGHT)
                move_right();
            else if (key == 'k' || key == KEY_ARROW_UP)
                move_vertical(false);
            else if (key == 'j' || key == KEY_ARROW_DOWN)
                move_vertical(true);
            else if (key == 'x' && cursor < length)
            {
                for (unsigned int i = cursor; i + 1 < length; ++i)
                    text[i] = text[i + 1];
                --length;
                text[length] = '\0';
                dirty = true;
            }
        }
    }

    clear_screen();
    vga_restore_screen(screen_snapshot);
    terminal_set_output_position(old_position);
}
