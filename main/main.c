#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/* Funciones escritas en assembler RISC-V (archivo led.S).
 * Manejan el LED de la placa (GPIO8) escribiendo directo en los
 * registros del GPIO, sin pasar por el driver de ESP-IDF. */
void led_iniciar(void);
void led_encender(void);
void led_apagar(void);

/* ------------------------------------------------------------------
 * CONEXION DEL JOYSTICK (modulo tipico KY-023) A LA ESP32-C3
 *
 *   Joystick   ->  ESP32-C3
 *   GND        ->  GND
 *   +5V / VCC  ->  3V3   (OJO: a 3.3 V, no a 5 V. Si lo alimentas con 5 V
 *                         la salida pasa de 3.3 V y la lectura no sirve)
 *   VRx        ->  GPIO0 (ADC1 canal 0)
 *   VRy        ->  GPIO1 (ADC1 canal 1)
 *   SW         ->  GPIO3 (boton de reinicio)
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

/* Poner en 1 y recompilar para ver el ADC y los intentos de movimiento
 * en el monitor serie. En 0 se usa la salida normal del juego. */
#ifndef DEBUG_LABERINTO
#define DEBUG_LABERINTO 0
#endif

#if DEBUG_LABERINTO
#define DEBUG_LOG(...) printf("[debug] " __VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

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
    /* \033[H vuelve el cursor arriba de todo y escribimos ENCIMA del
     * laberinto anterior. Antes usabamos \033[2J (borrar toda la
     * pantalla) en cada paso: la consola guardaba cada pantalla vieja
     * en el historial y se iba poniendo cada vez mas lenta. */
    const char *borrar_linea = DEBUG_LABERINTO ? "" : "\033[K";
    if (!DEBUG_LABERINTO)
    {
        printf("\033[H");
    }
    printf("Mueve el joystick para jugar. Boton o tecla R para reiniciar.%s\n\n",
           borrar_linea);

    for (int i = 0; i < ALTO; i++)
    {
        printf("%s%s\n", laberinto[i], borrar_linea);
    }

    printf("\nMovimientos validos: %d%s\n", movimientos, borrar_linea);
    if (!DEBUG_LABERINTO)
    {
        printf("\033[J");   /* borra los avisos que quedaron mas abajo */
    }
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

    led_apagar();        /* partida nueva: LED apagado */
    if (!DEBUG_LABERINTO)
    {
        printf("\033[2J");   /* solo al reiniciar borramos toda la pantalla */
    }
    DEBUG_LOG("Partida reiniciada. Posicion=(%d,%d) validos=%d\n",
              px, py, movimientos);
    imprimir_laberinto();
}

void mover(int dx, int dy)
{
    if (terminado)
    {
        printf("\nPartida terminada. Boton o tecla R para reiniciar\n");
        fflush(stdout);
        return;
    }

    int nx = px + dx;
    int ny = py + dy;

    if (laberinto[ny][nx] == '|')
    {
        DEBUG_LOG("Movimiento bloqueado por pared. (%d,%d) -> (%d,%d) validos=%d\n",
                  px, py, nx, ny, movimientos);

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

        /* Parpadeo corto del LED en cada choque */
        led_encender();
        vTaskDelay(150 / portTICK_PERIOD_MS);
        led_apagar();
        return;
    }

    /* Movimiento valido: la proxima vez que choques se vuelve a avisar */
    aviso_mostrado = 0;
    DEBUG_LOG("Movimiento valido. (%d,%d) -> (%d,%d) validos=%d\n",
              px, py, nx, ny, movimientos + 1);

    if (laberinto[ny][nx] == 'M')
    {
        laberinto[py][px] = ' ';
        px = nx;
        py = ny;
        laberinto[py][px] = 'P';
        movimientos++;
        terminado = 1;

        led_encender();   /* meta: el LED queda prendido hasta reiniciar */
        imprimir_laberinto();
        printf("\nLaberinto completado\n");
        printf("Cantidad de movimientos validos: %d\n", movimientos);
        printf("Boton o tecla R para reiniciar\n");
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

/* Configura el ADC1 para los ejes y una entrada digital para el boton. */
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

    /* SW se conecta a GND al presionar. El pull-up mantiene el pin en 1
     * cuando se suelta el boton o se deja SW desconectado. */
    gpio_config_t cfg_boton = {
        .pin_bit_mask = 1ULL << JOY_PIN_BOTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg_boton));
}

static bool boton_reinicio_pulsado(void)
{
    static bool lectura_anterior;
    static bool estado_estable;
    bool presionado = gpio_get_level(JOY_PIN_BOTON) == 0;

    /* Dos lecturas seguidas iguales filtran el rebote. Solo el paso de
     * suelto a presionado dispara el reinicio. */
    bool cambio_estable = presionado == lectura_anterior && presionado != estado_estable;
    lectura_anterior = presionado;
    if (cambio_estable)
    {
        estado_estable = presionado;
    }
    return cambio_estable && presionado;
}

typedef enum
{
    CENTRO,
    ARRIBA,
    ABAJO,
    IZQUIERDA,
    DERECHA
} Direccion;

/* Convierte las lecturas del ADC en una direccion del joystick. */
static Direccion leer_joystick(void)
{
    int x = 0;
    int y = 0;
    adc_oneshot_read(adc, JOY_CANAL_X, &x);
    adc_oneshot_read(adc, JOY_CANAL_Y, &y);

    /* Si en tu joystick los ejes salen al reves (arriba mueve abajo,
     * etc.), intercambia las direcciones de estas lineas. */
    Direccion direccion = CENTRO;
    if (y < UMBRAL_BAJO) direccion = ARRIBA;
    else if (y > UMBRAL_ALTO) direccion = ABAJO;
    else if (x < UMBRAL_BAJO) direccion = IZQUIERDA;
    else if (x > UMBRAL_ALTO) direccion = DERECHA;

#if DEBUG_LABERINTO
    /* Mostrar los cambios de direccion enseguida y una muestra por segundo
     * si se mantiene la direccion o el joystick sigue en el centro. */
    static Direccion direccion_anterior = CENTRO;
    static bool primera_muestra = true;
    static TickType_t ultima_muestra;
    TickType_t ahora = xTaskGetTickCount();
    if (primera_muestra || direccion != direccion_anterior ||
        ahora - ultima_muestra >= pdMS_TO_TICKS(1000))
    {
        const char *nombre = "centro";
        switch (direccion)
        {
            case CENTRO: break;
            case ARRIBA: nombre = "arriba"; break;
            case ABAJO: nombre = "abajo"; break;
            case IZQUIERDA: nombre = "izquierda"; break;
            case DERECHA: nombre = "derecha"; break;
        }
        DEBUG_LOG("ADC x=%d y=%d direccion=%s posicion=(%d,%d)\n",
                  x, y, nombre, px, py);
        direccion_anterior = direccion;
        ultima_muestra = ahora;
        primera_muestra = false;
    }
#endif

    return direccion;
}

static void mover_segun_direccion(Direccion direccion)
{
    switch (direccion)
    {
        case CENTRO: break;
        case ARRIBA: mover(0, -1); break;
        case ABAJO: mover(0, 1); break;
        case IZQUIERDA: mover(-1, 0); break;
        case DERECHA: mover(1, 0); break;
    }
}

void app_main(void)
{
    /* Sin buffer: cada tecla llega apenas se aprieta */
    setvbuf(stdin, NULL, _IONBF, 0);

    iniciar_joystick();
    led_iniciar();
    reiniciar();

    Direccion anterior = CENTRO;

    while (1)
    {
        /* 1) Tecla R para reiniciar. getchar() NO espera: si no se
         *    apreto nada devuelve EOF enseguida y seguimos con el
         *    joystick. Cualquier otra tecla se ignora. */
        int tecla = getchar();
        clearerr(stdin);
        bool pulsacion = boton_reinicio_pulsado();

        /* 2) Joystick. Solo movemos cuando cambia de posicion, asi un
         *    empujon = un paso. Para dar otro paso hay que volver al
         *    centro (o mover hacia otro lado). */
        Direccion direccion = leer_joystick();
        if (tecla == 'r' || tecla == 'R' || pulsacion)
        {
            reiniciar();
        }
        else if (direccion != CENTRO && direccion != anterior)
        {
            mover_segun_direccion(direccion);
        }
        anterior = direccion;

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
