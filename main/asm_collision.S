    .text
    .align 2
    .global asm_check_cell
    .type asm_check_cell, @function

/*
 * int asm_check_cell(const uint8_t *maze, int width, int height, int x, int y)
 *
 * RISC-V ABI:
 * a0 = maze base pointer
 * a1 = width
 * a2 = height
 * a3 = x
 * a4 = y
 *
 * The C matrix maze[y][x] is stored row by row. The linear offset is:
 * offset = y * width + x
 *
 * Return values:
 * 0 = blocked
 * 1 = free
 * 2 = goal
 */
asm_check_cell:
    blt     a3, zero, .Lblocked
    blt     a4, zero, .Lblocked
    bge     a3, a1, .Lblocked
    bge     a4, a2, .Lblocked

    mul     t0, a4, a1
    add     t0, t0, a3
    add     t1, a0, t0
    lbu     t2, 0(t1)

    li      t3, 1
    beq     t2, t3, .Lblocked

    li      t3, 0
    beq     t2, t3, .Lfree

    li      t3, 2
    beq     t2, t3, .Lgoal

.Lblocked:
    li      a0, 0
    ret

.Lfree:
    li      a0, 1
    ret

.Lgoal:
    li      a0, 2
    ret

    .size asm_check_cell, .-asm_check_cell
