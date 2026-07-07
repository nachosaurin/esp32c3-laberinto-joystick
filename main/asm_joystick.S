    .text
    .align 2
    .global asm_process_joystick
    .type asm_process_joystick, @function

/*
 * int asm_process_joystick(int adc_x, int adc_y, int center_x, int center_y, int deadzone)
 *
 * RISC-V ABI:
 * a0 = adc_x
 * a1 = adc_y
 * a2 = center_x
 * a3 = center_y
 * a4 = deadzone
 *
 * Return values:
 * 0 = DIR_NONE
 * 1 = DIR_UP
 * 2 = DIR_DOWN
 * 3 = DIR_LEFT
 * 4 = DIR_RIGHT
 */
asm_process_joystick:
    sub     t0, a0, a2
    sub     t1, a1, a3

    mv      t2, t0
    bge     t2, zero, .Labs_dx_ready
    neg     t2, t2
.Labs_dx_ready:

    mv      t3, t1
    bge     t3, zero, .Labs_dy_ready
    neg     t3, t3
.Labs_dy_ready:

    blt     t2, a4, .Lmaybe_none_y
    j       .Lchoose_axis

.Lmaybe_none_y:
    blt     t3, a4, .Ldir_none

.Lchoose_axis:
    blt     t3, t2, .Lx_axis

    blt     zero, t1, .Ldir_down
    blt     t1, zero, .Ldir_up
    j       .Ldir_none

.Lx_axis:
    blt     zero, t0, .Ldir_right
    blt     t0, zero, .Ldir_left
    j       .Ldir_none

.Ldir_none:
    li      a0, 0
    ret

.Ldir_up:
    li      a0, 1
    ret

.Ldir_down:
    li      a0, 2
    ret

.Ldir_left:
    li      a0, 3
    ret

.Ldir_right:
    li      a0, 4
    ret

    .size asm_process_joystick, .-asm_process_joystick
