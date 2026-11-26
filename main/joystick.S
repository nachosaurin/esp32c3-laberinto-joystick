    .section .text
    .align  2
    .global joystick_direction_asm
    .type   joystick_direction_asm, @function

/*
 * int joystick_direction_asm(
 *     int x,         a0
 *     int y,         a1
 *     int center_x,  a2
 *     int center_y,  a3
 *     int deadzone   a4
 * );
 *
 * Retorno:
 * 0 = NONE, 1 = UP, 2 = DOWN, 3 = LEFT, 4 = RIGHT
 */
joystick_direction_asm: //para calcualar cuanto nos alejamos del centro
    sub     t0, a0, a2          /* dx = x - center_x */
    sub     t1, a1, a3          /* dy = y - center_y */

    /* t2 = abs(dx) */
    bge     t0, zero, 1f //si ya es mayor que cero, se va a 1 y lo copia a t2
    neg     t2, t0 //si era negativo, le invierte el signo y despues lo guarda en t2
    j       2f
1:
    mv      t2, t0
2:
    /* t3 = abs(dy) */ //hace lo mismo que en dx
    bge     t1, zero, 3f
    neg     t3, t1
    j       4f
3:
    mv      t3, t1
4:
    /* Si abs(dx) <= deadzone y abs(dy) <= deadzone: DIR_NONE */
    blt     a4, t2, 5f //deadzone <= t2 = abs(dx)
    blt     a4, t3, 5f //deadzone <= t3 = abs(dy)
    li      a0, 0
    ret

5:
    /* Domina X si abs(dx) >= abs(dy); en empate se elige X. */
    bge     t2, t3, 6f

    /* Domina Y */
    blt     t1, zero, 7f
    li      a0, 2              /* DIR_DOWN */
    ret
7:
    li      a0, 1              /* DIR_UP */
    ret

6:
    /* Domina X */
    blt     t0, zero, 8f
    li      a0, 4              /* DIR_RIGHT */
    ret
8:
    li      a0, 3              /* DIR_LEFT */
    ret

    .size joystick_direction_asm, .-joystick_direction_asm