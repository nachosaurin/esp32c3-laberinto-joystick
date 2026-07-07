#ifndef MAZE_H
#define MAZE_H

#include <stdbool.h>
#include <stdint.h>

#define MAZE_WIDTH 10
#define MAZE_HEIGHT 8

#define CELL_FREE ((uint8_t)0u)
#define CELL_WALL ((uint8_t)1u)
#define CELL_GOAL ((uint8_t)2u)

#define MAZE_START_X 1
#define MAZE_START_Y 1

typedef struct {
    int player_x;
    int player_y;
    int start_x;
    int start_y;
    int moves_count;
    bool game_completed;
} GameState;

extern const uint8_t maze[MAZE_HEIGHT][MAZE_WIDTH];

void maze_reset_game(GameState *state);
const uint8_t *maze_get_base(void);
uint8_t maze_get_cell(int x, int y);

#endif
