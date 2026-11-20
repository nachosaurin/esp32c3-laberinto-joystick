#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/* ------------------------------------------------------------------
 * CONEXION DEL JOYSTICK (modulo tipico KY-023) A LA ESP32-C3
 *
 *   Joystick   ->  ESP32-C3
 *   GND        ->  GND
 *   +5V / VCC  ->  3V3   (OJO: a 3.3 V, no a 5 V. Si lo alimentas con 5 V
 *                         la salida pasa de 3.3 V y la lectura no sirve)
 *   VRx        ->  GPIO0 (ADC1 canal 0)
 *   VRy        ->  GPIO1 (ADC1 canal 1)
 *   SW         ->  GPIO3 (boton, se usa para reiniciar)
 *
 * En la ESP32-C3 solo el ADC1 (GPIO0 a GPIO4) es confiable para leer
 * valores analogicos. Si usas otros pines, cambia los defines de abajo.
 * ------------------------------------------------------------------ */
#define JOY_CANAL_X   ADC_CHANNEL_0   /* GPIO0 */
#define JOY_CANAL_Y   ADC_CHANNEL_1   /* GPIO1 */
#define JOY_PIN_BOTON GPIO_NUM_3

/* El ADC devuelve un numero entre 0 y 4095. Con el joystick quieto
 * da mas o menos 2048 (el centro). Si baja de UMBRAL_BAJO o sube de
 * UMBRAL_ALTO consideramos que lo inclinaste hacia ese lado. */
#define UMBRAL_BAJO   1000
#define UMBRAL_ALTO   3000

/* Pone 0 si todavia no conectaste el joystick. Con los pines al aire
 * el ADC lee valores al azar y el jugador se "mueve solo" (y aparecen
 * muchos carteles de Movimiento bloqueado). Con 0 se juega solo con
 * el teclado. */
#define USAR_JOYSTICK 1

#define ANCHO 20
#define ALTO 10

/* Copia original del laberinto: se usa para reiniciar. Asi el mapa
 * inicial y el de reinicio siempre son iguales. Cada fila tiene
 * exactamente ANCHO (20) caracteres. */
static const char laberinto_inicial[ALTO][ANCHO + 1] = {
    "||||||||||||||||||||",
    "|P              |  |",
    "| |||| | |||||| | ||",
    "| |    |      | |  |",
    "| | |||||||| | ||| |",
    "| |        | |     |",
    "| |||||| | | |||||||",
    "|      | | |       |",
    "|||| |   | |||||| M|",
    "||||||||||||||||||||"
};

char laberinto[ALTO][ANCHO + 1];

int px = 1;
int py = 1;
int movimientos = 0;
int terminado = 0;
int aviso_mostrado = 0;   /* 1 si ya se mostro "Movimiento bloqueado" */

static adc_oneshot_unit_handle_t adc;

void imprimir_laberinto()
{
    printf("\033[2J\033[H");
    printf("Mueve el joystick (o usa W A S D). Boton del joystick o R para reiniciar.\n\n");

    for (int i = 0; i < ALTO; i++)
    {
        printf("%s\n", laberinto[i]);
    }

    printf("\nMovimientos validos: %d\n", movimientos);
    fflush(stdout);
}

void reiniciar()
{
    memcpy(laberinto, laberinto_inicial, sizeof(laberinto));

    px = 1;
    py = 1;
    movimientos = 0;
    terminado = 0;
    aviso_mostrado = 0;

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
        /* El aviso se muestra una sola vez. Si seguis chocando la
         * misma pared no se vuelve a escribir; se vuelve a mostrar
         * recien despues de un movimiento valido. */
        if (!aviso_mostrado)
        {
            imprimir_laberinto();
            printf("\nMovimiento bloqueado\n");
            fflush(stdout);
            aviso_mostrado = 1;
        }
        return;
    }

    /* Movimiento valido: la proxima vez que choques se vuelve a avisar */
    aviso_mostrado = 0;

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

/* Configura el ADC1 para leer los dos ejes y el GPIO del boton. */
void iniciar_joystick()
{
    adc_oneshot_unit_init_cfg_t cfg_unidad = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&cfg_unidad, &adc));

    /* ADC_ATTEN_DB_12 permite medir de 0 V hasta ~3.3 V */
    adc_oneshot_chan_cfg_t cfg_canal = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, JOY_CANAL_X, &cfg_canal));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, JOY_CANAL_Y, &cfg_canal));

    /* El boton une SW con GND al apretarlo: activamos la resistencia
     * pull-up interna para que sin apretar se lea 1 y apretado 0. */
    gpio_config_t cfg_boton = {
        .pin_bit_mask = 1ULL << JOY_PIN_BOTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg_boton));
}

/* Lee el joystick y devuelve la "tecla" equivalente:
 * 'w', 'a', 's', 'd', 'r' (boton) o 0 si esta en el centro. */
int leer_joystick()
{
    int x = 0;
    int y = 0;
    adc_oneshot_read(adc, JOY_CANAL_X, &x);
    adc_oneshot_read(adc, JOY_CANAL_Y, &y);

    if (gpio_get_level(JOY_PIN_BOTON) == 0) return 'r';

    /* Si en tu joystick los ejes salen al reves (arriba mueve abajo,
     * etc.), intercambia las letras de estas lineas. */
    if (y < UMBRAL_BAJO) return 'w';
    if (y > UMBRAL_ALTO) return 's';
    if (x < UMBRAL_BAJO) return 'a';
    if (x > UMBRAL_ALTO) return 'd';

    return 0;
}

void procesar_tecla(int tecla)
{
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
}

void app_main(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);

    if (USAR_JOYSTICK)
    {
        iniciar_joystick();
    }
    reiniciar();

    int anterior = 0;

    while (1)
    {
        /* 1) Teclado (por la consola serie). Si no hay nada, getchar
         *    devuelve EOF y no hacemos nada. */
        int tecla = getchar();
        if (tecla != EOF)
        {
            procesar_tecla(tecla);
        }
        clearerr(stdin);

        /* 2) Joystick. Solo movemos cuando cambia de posicion, asi un
         *    empujon = un paso. Para dar otro paso hay que volver al
         *    centro (o mover hacia otro lado). */
        int joy = USAR_JOYSTICK ? leer_joystick() : 0;
        if (joy != 0 && joy != anterior)
        {
            procesar_tecla(joy);
        }
        anterior = joy;

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
