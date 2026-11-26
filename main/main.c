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
 * ------------------------------------------------------------------ */
#define JOY_CANAL_X   ADC_CHANNEL_0   /* GPIO0 */
#define JOY_CANAL_Y   ADC_CHANNEL_1   /* GPIO1 */
#define JOY_PIN_BOTON GPIO_NUM_3

/* El ADC devuelve un numero entre 0 y 4095. Con el joystick quieto
 * da mas o menos 2048 (el centro). Si baja de UMBRAL_BAJO o sube de
 * UMBRAL_ALTO consideramos que lo inclinaste hacia ese lado. */
#define UMBRAL_BAJO   1000
#define UMBRAL_ALTO   3000

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
    "| | ||| |||| |  || |",
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

static adc_oneshot_unit_handle_t adc; //guarda la referencia al conversor analogico para poder leerlo
                                    // desde cualquier funcion


void imprimir_laberinto()
{
    /* \033[H vuelve el cursor arriba de todo y escribimos ENCIMA del
     * laberinto anterior. Antes usabamos \033[2J (borrar toda la
     * pantalla) en cada paso: la consola guardaba cada pantalla vieja
     * en el historial y se iba poniendo cada vez mas lenta. */
    printf("\033[H"); //mueve el cursor a la esquina inicial y va actualizando la posicion del jugador
    printf("Mueve el joystick para jugar. Boton o tecla R para reiniciar.\033[K\n\n");

    for (int i = 0; i < ALTO; i++) //vamos recorriendo las filas hasta la ultima. cuando 1 = 10 se termina
    {
        printf("%s\033[K\n", laberinto[i]);   /* \033[K borra el resto de la linea */
    }

    printf("\nMovimientos validos: %d\033[K\n", movimientos);
    printf("\033[J");   /* borra lo que haya quedado mas abajo (avisos viejos) */
    fflush(stdout);
}

void reiniciar() //deja todo como estaba antes de arrancar el juego. Ahora solo borra la pantalla
                // al reiniciar el juego (no hacemos el truco del cursor)
{
    memcpy(laberinto, laberinto_inicial, sizeof(laberinto));

    px = 1;
    py = 1;
    movimientos = 0;
    terminado = 0;
    aviso_mostrado = 0;

    led_apagar();        /* partida nueva: LED apagado */
    printf("\033[2J");   /* solo al reiniciar borramos toda la pantalla */
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
        /* El aviso se muestra una sola vez. Si seguis chocando la
         * misma pared no se vuelve a escribir; se vuelve a mostrar
         * recien despues de un movimiento valido. */
        if (!aviso_mostrado) //hace que si te quedas chocanco contra la pared varias veces, la  
                            // consola no se llena de carteles repetidos
        {
            imprimir_laberinto();
            printf("\nMovimiento bloqueado\n");
            fflush(stdout);
            aviso_mostrado = 1;
        }

        /* Parpadeo corto del LED en cada choque */
        led_encender(); //rutina de assembler --> pone el pin en bajo --> 0
        vTaskDelay(150 / portTICK_PERIOD_MS); //lo prende por 150 ms. Es mejor que el delay() porque puede poner 
                                              //a dormir una tarea por 150 ms y liberador procesador para otra cosa

        led_apagar(); //rutina de assebmbler --> pone el pin en alto --> 1
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
        .unit_id = ADC_UNIT_1, // enciende el conversor de señales analogicas a digitales.  
                                //one-shot porque  toma una lectura puntal cada vez que necesitamos
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
    gpio_config_t cfg_boton = { //esto es para config del SW
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
    static bool lectura_anterior; //guarda lo que lei la ultima vez
    static bool estado_estable; //guarda si el boton ya estaba suelto o apretdo 
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

/* Lee el joystick y devuelve una letra segun hacia donde esta inclinado:
 * 'w', 'a', 's', 'd' o 0 si esta en el centro. */
int leer_joystick()
{
    int x = 0;
    int y = 0;
    adc_oneshot_read(adc, JOY_CANAL_X, &x);
    adc_oneshot_read(adc, JOY_CANAL_Y, &y);

    /* Si en tu joystick los ejes salen al reves (arriba mueve abajo,
     * etc.), intercambia las letras de estas lineas. */
    if (y < UMBRAL_BAJO) return 'w';
    if (y > UMBRAL_ALTO) return 's';
    if (x < UMBRAL_BAJO) return 'a';
    if (x > UMBRAL_ALTO) return 'd';

    return 0;
}

void procesar_accion(int accion)
{
    if (accion == 'w')
    {
        mover(0, -1);
    }
    else if (accion == 's')
    {
        mover(0, 1);
    }
    else if (accion == 'a')
    {
        mover(-1, 0);
    }
    else if (accion == 'd')
    {
        mover(1, 0);
    }
    else if (accion == 'r')
    {
        reiniciar();
    }
}

void app_main(void)
{
    /* Sin buffer: cada tecla llega apenas se aprieta */
    setvbuf(stdin, NULL, _IONBF, 0);

    iniciar_joystick();
    led_iniciar();
    reiniciar();

    int anterior = 0;

    while (1) //lo unico que apaga la placa es que la desenchufe
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
        int joy = leer_joystick();
        if (tecla == 'r' || tecla == 'R' || pulsacion)//si no se apretan las 'r' o el sw, se mueve el personaje
        {
            procesar_accion('r');
        }
        else if (joy != 0 && joy != anterior)
        {
            procesar_accion(joy);
        }
        anterior = joy;

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
