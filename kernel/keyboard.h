#ifndef KEYBOARD_H
#define KEYBOARD_H

// ============================================================
// PS/2 KEYBOARD POLLING (UK SCANCODE SET 1)
// ============================================================

#define KEY_ARROW_UP    1
#define KEY_ARROW_DOWN  2
#define KEY_ARROW_LEFT  3
#define KEY_ARROW_RIGHT 4

// Polls the keyboard controller once.
// Returns an ASCII character for normal key presses.
// Returns KEY_ARROW_UP / KEY_ARROW_DOWN / KEY_ARROW_LEFT / KEY_ARROW_RIGHT
// for arrow keys.
// Returns 0 when no usable key input is available.
char keyboard_poll_char();

#endif
