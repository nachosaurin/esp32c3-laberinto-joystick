#include "uart_view.h"

#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "sdkconfig.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define TERMINAL_RX_BUFFER_SIZE 256

static bool s_uart_driver_ready;
static bool s_usb_jtag_driver_ready;

void uart_view_init_terminal(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

#if CONFIG_ESP_CONSOLE_UART
    if (!uart_is_driver_installed(CONFIG_ESP_CONSOLE_UART_NUM)) {
        esp_err_t uart_result = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                                                    TERMINAL_RX_BUFFER_SIZE,
                                                    0,
                                                    0,
                                                    NULL,
                                                    0);
        s_uart_driver_ready = (uart_result == ESP_OK);
    } else {
        s_uart_driver_ready = true;
    }

    if (s_uart_driver_ready) {
        uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
        uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
        uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    }
#endif

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED || CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        esp_err_t usb_result = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
        s_usb_jtag_driver_ready = (usb_result == ESP_OK);
    } else {
        s_usb_jtag_driver_ready = true;
    }

    if (s_usb_jtag_driver_ready) {
        usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
        usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
        usb_serial_jtag_vfs_use_driver();
    }
#endif

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
}

int uart_view_read_command(void)
{
    uint8_t received = 0;

#if CONFIG_ESP_CONSOLE_UART
    if (s_uart_driver_ready &&
        uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &received, 1, 0) == 1) {
        return received;
    }
#endif

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED || CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG
    if (s_usb_jtag_driver_ready &&
        usb_serial_jtag_read_bytes(&received, 1, 0) == 1) {
        return received;
    }
#endif

    int fallback = getchar();
    if (fallback != EOF) {
        return fallback;
    }

    return EOF;
}

void uart_view_print_welcome(void)
{
    printf("Proyecto 24 - Laberinto con joystick en ESP32-C3\n");
    printf("Joystick HW-504 o modo prueba UART para mover al jugador P.\n");
    printf("Objetivo: llegar a G sin atravesar paredes #.\n");
    printf("Modo prueba UART: escriba W/A/S/D y presione Enter. R reinicia.\n\n");
}

void uart_view_print_maze(const GameState *state)
{
    printf("\n--- Laberinto ---\n");
    printf("Leyenda: P=jugador  #=pared  .=camino  G=meta\n");
    printf("Posicion: x=%d, y=%d | Movimientos validos: %d\n",
           state != NULL ? state->player_x : 0,
           state != NULL ? state->player_y : 0,
           state != NULL ? state->moves_count : 0);
    printf("     x: 0123456789\n");

    for (int y = 0; y < MAZE_HEIGHT; ++y) {
        printf("y=%d     ", y);
        for (int x = 0; x < MAZE_WIDTH; ++x) {
            if (state != NULL && state->player_x == x && state->player_y == y) {
                putchar('P');
                continue;
            }

            switch (maze_get_cell(x, y)) {
            case CELL_WALL:
                putchar('#');
                break;
            case CELL_GOAL:
                putchar('G');
                break;
            case CELL_FREE:
            default:
                putchar('.');
                break;
            }
        }
        putchar('\n');
    }

    printf("Comando: W=arriba, A=izquierda, S=abajo, D=derecha, R=reiniciar\n\n");
}

void uart_view_print_blocked(int x, int y)
{
    printf("Movimiento bloqueado en destino (%d, %d)\n", x, y);
}

void uart_view_print_victory(int moves_count)
{
    printf("Laberinto completado\n");
    printf("Movimientos validos: %d\n", moves_count);
    printf("Escriba R para reiniciar\n");
}

void uart_view_print_restart(void)
{
    printf("Partida reiniciada\n");
}

void uart_view_print_calibration(const JoystickCalibration *calibration)
{
    if (calibration == NULL) {
        return;
    }

    printf("Joystick calibrado: centro X=%d, centro Y=%d\n\n",
           calibration->center_x, calibration->center_y);
}
