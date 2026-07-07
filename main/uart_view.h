#ifndef UART_VIEW_H
#define UART_VIEW_H

#include "joystick.h"
#include "maze.h"

void uart_view_init_terminal(void);
int uart_view_read_command(void);
void uart_view_print_welcome(void);
void uart_view_print_maze(const GameState *state);
void uart_view_print_blocked(int x, int y);
void uart_view_print_victory(int moves_count);
void uart_view_print_restart(void);
void uart_view_print_calibration(const JoystickCalibration *calibration);

#endif
