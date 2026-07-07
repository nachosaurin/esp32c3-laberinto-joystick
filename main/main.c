#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ANCHO 20
#define ALTO 10

char laberinto[ALTO][ANCHO + 1] = {
    "||||||||||||||||||||",
    "|P             |  |",
    "| |||| | |||||| | ||",
    "| |    |      | |  |",
    "| | |||||||| | ||| |",
    "| |        | |     |",
    "| |||||| | | |||||||",
    "|      | | |       |",
    "|||| |   | |||||| M|",
    "||||||||||||||||||||"
};

int px = 1;
int py = 1;
int movimientos = 0;
int terminado = 0;

void imprimir_laberinto()
{
    printf("\033[2J\033[H");
    printf("Usa W A S D para moverte. R para reiniciar.\n\n");

    for (int i = 0; i < ALTO; i++)
    {
        printf("%s\n", laberinto[i]);
    }

    printf("\nMovimientos validos: %d\n", movimientos);
    fflush(stdout);
}

void reiniciar()
{
    strcpy(laberinto[0], "||||||||||||||||||||");
    strcpy(laberinto[1], "|P             |  |");
    strcpy(laberinto[2], "| |||| | |||||| | ||");
    strcpy(laberinto[3], "| |          | |  |");
    strcpy(laberinto[4], "| | |||||||| | ||| |");
    strcpy(laberinto[5], "| |        | |     |");
    strcpy(laberinto[6], "| |||||| | | |||||||");
    strcpy(laberinto[7], "|      | | |       |");
    strcpy(laberinto[8], "|||| |   | |||||| M|");
    strcpy(laberinto[9], "||||||||||||||||||||");

    px = 1;
    py = 1;
    movimientos = 0;
    terminado = 0;

    imprimir_laberinto();
}

void mover(int dx, int dy)
{
    if (terminado)
    {
        printf("\nPartida terminada. Presiona R para reiniciar\n");
        fflush(stdout);
        return;
    }

    int nx = px + dx;
    int ny = py + dy;

    if (laberinto[ny][nx] == '|')
    {
        printf("\nMovimiento bloqueado\n");
        fflush(stdout);
        return;
    }

    if (laberinto[ny][nx] == 'M')
    {
        laberinto[py][px] = ' ';
        px = nx;
        py = ny;
        laberinto[py][px] = 'P';
        movimientos++;
        terminado = 1;

        imprimir_laberinto();
        printf("\nLaberinto completado\n");
        printf("Cantidad de movimientos validos: %d\n", movimientos);
        printf("Presiona R para reiniciar\n");
        fflush(stdout);
        return;
    }

    laberinto[py][px] = ' ';
    px = nx;
    py = ny;
    laberinto[py][px] = 'P';
    movimientos++;

    imprimir_laberinto();
}

void app_main(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);

    reiniciar();

    while (1)
    {
        int tecla = getchar();

        if (tecla == 'w' || tecla == 'W')
        {
            mover(0, -1);
        }
        else if (tecla == 's' || tecla == 'S')
        {
            mover(0, 1);
        }
        else if (tecla == 'a' || tecla == 'A')
        {
            mover(-1, 0);
        }
        else if (tecla == 'd' || tecla == 'D')
        {
            mover(1, 0);
        }
        else if (tecla == 'r' || tecla == 'R')
        {
            reiniciar();
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}