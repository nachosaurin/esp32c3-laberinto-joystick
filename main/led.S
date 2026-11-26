/* ------------------------------------------------------------------
 * led.S: manejo del LED de la placa (GPIO8) en assembler RISC-V.
 *
 * Accedemos directo a los registros del GPIO y del IO MUX de la
 * ESP32-C3, sin usar el driver de ESP-IDF. Direcciones sacadas del
 * Technical Reference Manual (y de soc/gpio_reg.h, soc/io_mux_reg.h).
 *
 * En la ESP32-C3 Super Mini el LED azul esta en GPIO8 y es ACTIVO EN
 * BAJO: se prende cuando el pin vale 0 y se apaga cuando vale 1.
 *
 * Convencion de llamada RISC-V: estas funciones no reciben parametros
 * ni devuelven nada, y solo usan registros temporales (t0, t1, t2),
 * que C no espera que se conserven. Por eso no hace falta usar la pila.
 * ------------------------------------------------------------------ */

#define IO_MUX_GPIO8_REG            0x60009024  /* la pata fisica numero 8 funcione como GPIO */
#define GPIO_OUT_W1TS_REG           0x60004008  /* poner en 1 bits de salida */
#define GPIO_OUT_W1TC_REG           0x6000400C  /* poner en 0 bits de salida */
#define GPIO_ENABLE_W1TS_REG        0x60004024  /* habilitar pines como salida. Al principio los pines estan configurados como entradas (alta impedancia) */
#define GPIO_FUNC8_OUT_SEL_CFG_REG  0x60004574  /* aca le decimos al pin 8 que es lo que queremos que salga
le enviamos el 128 (que salga el valor del registro del gpio) y se prende el bit 9 (el permiso para emitir depende del gpio_enable*/

#define LED_MASK   (1 << 8) /* bit 8 = GPIO8. Es para enviarle a w1tcs, w1tc o enable que la orden va dirigida al pin (bit) 8, los 31 restantes quedan igual */

    .section .text

/* void led_iniciar(void)
 * Configura GPIO8 como salida y deja el LED apagado. */
    .global led_iniciar //declaro funcion publica

    .type   led_iniciar, @function //con esto digo que es una funcion (y no una variable)

led_iniciar:
    /* 1) IO MUX: campo MCU_SEL (bits 14..12) = 1 -> el pin funciona como GPIO */

    /*
     31                               15  14  13  12  11                               0
┌───────────────────────────────────┬───────────┬───────────────────────────────────┐
│     Otros bits de control         │  MCU_SEL  │     Otros bits de control         │
│  (Filtros, drive strength, etc.)  │ (3 bits)  │   (Pull-up, Pull-down, etc.)      │
└───────────────────────────────────┴───────────┴───────────────────────────────────┘
               17 bits                  3 bits                  12 bits
    
    */
    li   t0, IO_MUX_GPIO8_REG //aca guardamos la direccion de memoria fisica del mux del pin 8. t0 es el puntero
    lw   t1, 0(t0)              /* leer valor actual */
    li   t2, ~(0x7 << 12) //tomo el 111 y lo mando al fondo derecha, despues lo desplazo 12 lugraes y me queda en el 12 13 14
    and  t1, t1, t2            //al hacer un and aca me queda todo en 000 (los que nos interesan). El resto puede quedar en X
    li   t2, (1 << 12)      //tomo el 1 y lo muevo 12 lugares hacia la izquierda en t2.
    or   t1, t1, t2             /* aca lo pegamos en t1 */
    sw   t1, 0(t0) //escribimos los 32 bits en t1. nos queda asi x...001...x

    /* 2) Matriz GPIO: por el pin sale la senal 128 (salida GPIO simple)
     *    y el bit 9 (OEN_SEL) hace que la habilitacion venga de GPIO_ENABLE */
    li   t0, GPIO_FUNC8_OUT_SEL_CFG_REG //cargamos en el registro puntero que declaramos antes, la direc fisica del registro del pin 8
    li   t1, (128 | (1 << 9)) //el 128 es salida simple de GPIO por software (el valor de GPIO_OUT_W1TS y GPIO_OUT_W1TC) 
    /*
Bit: ...   10   9   8   7   6   5   4   3   2   1   0
128:        0   0   0   1   0   0   0   0   0   0   0   (código 128)
1 << 9:     0   1   0   0   0   0   0   0   0   0   0   (OEN_SEL = 1)
───────────────────────────────────────────────────────
Resultado:   0   1   0   1   0   0   0   0   0   0   0   (en decimal: 640) 
    
    Esto es lo que guardamos en t1 ahora
    */
    
    
    sw   t1, 0(t0) //aca usamos sw porque no nos interesa conservar este valor luego.

    /* 3) Dejar el pin en 1 (LED apagado) antes de habilitarlo */
    li   t1, LED_MASK //cargamos el numero de 32 bits con un 1 enn el bit 8. Funciona como un molde.
    li   t0, GPIO_OUT_W1TS_REG //cargamos la direccion del registro w1ts de la salida del gpio
    sw   t1, 0(t0) //aca lo que hacemos es poner el 1 en el bit 8 del registro. Todos 0 menos el 1 que metimos.

    /* 4) Habilitar GPIO8 como salida */
    li   t0, GPIO_ENABLE_W1TS_REG //vamos al registro de habilitacion ahora
    sw   t1, 0(t0)
    ret
    .size led_iniciar, . - led_iniciar //tamaño de la funcion en bytes

/* void led_encender(void)
 * Pin en 0 -> LED prendido (activo en bajo). */
    .global led_encender
    .type   led_encender, @function
led_encender:
    li   t0, GPIO_OUT_W1TC_REG  //el registro encargado de poner el pin en 0
    li   t1, LED_MASK //aca le decimos que ponga este pin en 1 --> eso hace que se prenda
                      // prenda, es decir que se ponga en 0 el pin 8
    sw   t1, 0(t0)
    ret
    .size led_encender, . - led_encender

/* void led_apagar(void)
 * Pin en 1 -> LED apagado. */
    .global led_apagar
    .type   led_apagar, @function
led_apagar:
    li   t0, GPIO_OUT_W1TS_REG
    li   t1, LED_MASK
    sw   t1, 0(t0)
    ret
    .size led_apagar, . - led_apagar
