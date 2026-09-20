#ifndef TERMINAL_H
#define TERMINAL_H

// ============================================================
// TERMINAL / LINE INPUT HANDLING
// ============================================================

void terminal_init();

void terminal_run();

// Returns the current cursor position.
unsigned short terminal_get_output_position();
void terminal_set_output_position(unsigned short position);

#endif
