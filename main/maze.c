#include "maze.h"

/*
 * The maze is a fixed rectangular matrix.
 * Coordinate convention:
 * x = column
 * y = row
 *
 * In memory this is stored linearly row by row, so maze[y][x] is equivalent
 * to base[y * MAZE_WIDTH + x]. The assembler collision routine uses that
 * exact addressing rule.
 */
const uint8_t maze[MAZE_HEIGHT][MAZE_WIDTH] = {
    {CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL},
    {CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_GOAL, CELL_WALL},
    {CELL_WALL, CELL_FREE, CELL_WALL, CELL_FREE, CELL_WALL, CELL_FREE, CELL_WALL, CELL_WALL, CELL_FREE, CELL_WALL},
    {CELL_WALL, CELL_FREE, CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_FREE, CELL_WALL, CELL_FREE, CELL_WALL},
    {CELL_WALL, CELL_FREE, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_FREE, CELL_WALL, CELL_FREE, CELL_WALL},
    {CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_FREE, CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_WALL},
    {CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_FREE, CELL_FREE, CELL_FREE, CELL_WALL, CELL_FREE, CELL_WALL},
    {CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL, CELL_WALL},
};

void maze_reset_game(GameState *state)
{
    if (state == NULL) {
        return;
    }

    state->start_x = MAZE_START_X;
    state->start_y = MAZE_START_Y;
    state->player_x = state->start_x;
    state->player_y = state->start_y;
    state->moves_count = 0;
    state->game_completed = false;
}

const uint8_t *maze_get_base(void)
{
    return &maze[0][0];
}

uint8_t maze_get_cell(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAZE_WIDTH || y >= MAZE_HEIGHT) {
        return CELL_WALL;
    }

    return maze[y][x];
}
