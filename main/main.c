#include <stdio.h>

#include "asm_functions.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "joystick.h"
#include "maze.h"
#include "uart_view.h"

#define MOVEMENT_DELAY_MS 200
#define IDLE_DELAY_MS 50
#define UART_KEYBOARD_TEST_MODE 1

static void apply_direction(int direction, int *next_x, int *next_y)
{
    switch (direction) {
    case DIR_UP:
        --(*next_y);
        break;
    case DIR_DOWN:
        ++(*next_y);
        break;
    case DIR_LEFT:
        --(*next_x);
        break;
    case DIR_RIGHT:
        ++(*next_x);
        break;
    case DIR_NONE:
    default:
        break;
    }
}

static void try_move_player(GameState *state, int direction)
{
    int next_x = state->player_x;
    int next_y = state->player_y;
    apply_direction(direction, &next_x, &next_y);

    int cell_result = asm_check_cell(maze_get_base(), MAZE_WIDTH, MAZE_HEIGHT, next_x, next_y);

    if (cell_result == 0) {
        uart_view_print_maze(state);
        uart_view_print_blocked(next_x, next_y);
        return;
    }

    state->player_x = next_x;
    state->player_y = next_y;
    ++state->moves_count;

    uart_view_print_maze(state);

    if (cell_result == 2) {
        state->game_completed = true;
        uart_view_print_victory(state->moves_count);
    }
}

static int direction_from_terminal_key(int received)
{
    switch (received) {
    case 'W':
    case 'w':
        return DIR_UP;
    case 'S':
    case 's':
        return DIR_DOWN;
    case 'A':
    case 'a':
        return DIR_LEFT;
    case 'D':
    case 'd':
        return DIR_RIGHT;
    default:
        return DIR_NONE;
    }
}

static bool try_restart_from_terminal(GameState *state, int received)
{
    if (received == 'R' || received == 'r') {
        maze_reset_game(state);
        uart_view_print_maze(state);
        uart_view_print_restart();
        return true;
    }

    return false;
}

void app_main(void)
{
    uart_view_init_terminal();
    uart_view_print_welcome();

    ESP_ERROR_CHECK(joystick_init());

    JoystickCalibration calibration;
    ESP_ERROR_CHECK(joystick_calibrate(&calibration, JOYSTICK_CALIBRATION_SAMPLES));
    uart_view_print_calibration(&calibration);

#if UART_KEYBOARD_TEST_MODE
    printf("Modo prueba sin joystick: use W/A/S/D para mover y R para reiniciar.\n\n");
#endif

    GameState state;
    maze_reset_game(&state);
    uart_view_print_maze(&state);

    while (true) {
        int terminal_char = uart_view_read_command();
        if (try_restart_from_terminal(&state, terminal_char)) {
            vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
            continue;
        }

        if (state.game_completed) {
            vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
            continue;
        }

#if UART_KEYBOARD_TEST_MODE
        int keyboard_direction = direction_from_terminal_key(terminal_char);
        if (keyboard_direction != DIR_NONE) {
            printf("Comando recibido: %c\n", terminal_char);
            try_move_player(&state, keyboard_direction);
            vTaskDelay(pdMS_TO_TICKS(MOVEMENT_DELAY_MS));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
#else
        JoystickReading reading;
        esp_err_t read_result = joystick_read_raw(&reading);
        if (read_result != ESP_OK) {
            printf("Error leyendo joystick: %s\n", esp_err_to_name(read_result));
            vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
            continue;
        }

        int direction = asm_process_joystick(reading.x, reading.y,
                                             calibration.center_x, calibration.center_y,
                                             JOYSTICK_DEFAULT_DEADZONE);

        if (direction == DIR_NONE) {
            vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
            continue;
        }

        try_move_player(&state, direction);
        vTaskDelay(pdMS_TO_TICKS(MOVEMENT_DELAY_MS));
#endif
    }
}
