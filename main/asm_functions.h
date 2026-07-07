#ifndef ASM_FUNCTIONS_H
#define ASM_FUNCTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns:
 * 0 = blocked cell or out of bounds
 * 1 = free cell
 * 2 = goal cell
 */
int asm_check_cell(const uint8_t *maze, int width, int height, int x, int y);

/*
 * Returns:
 * 0 = DIR_NONE
 * 1 = DIR_UP
 * 2 = DIR_DOWN
 * 3 = DIR_LEFT
 * 4 = DIR_RIGHT
 */
int asm_process_joystick(int adc_x, int adc_y, int center_x, int center_y, int deadzone);

#ifdef __cplusplus
}
#endif

#endif
