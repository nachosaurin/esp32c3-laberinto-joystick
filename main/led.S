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

#define IO_MUX_GPIO8_REG            0x60009024  /* IO MUX del pin 8 */
#define GPIO_OUT_W1TS_REG           0x60004008  /* poner en 1 bits de salida */
#define GPIO_OUT_W1TC_REG           0x6000400C  /* poner en 0 bits de salida */
#define GPIO_ENABLE_W1TS_REG        0x60004024  /* habilitar pines como salida */
#define GPIO_FUNC8_OUT_SEL_CFG_REG  0x60004574  /* que senal sale por el pin 8 */

#define LED_MASK   (1 << 8)        /* bit 8 = GPIO8 */

    .section .text

/* void led_iniciar(void)
 * Configura GPIO8 como salida y deja el LED apagado. */
    .global led_iniciar
    .type   led_iniciar, @function
led_iniciar:
    /* 1) IO MUX: campo MCU_SEL (bits 14..12) = 1 -> el pin funciona como GPIO */
    li   t0, IO_MUX_GPIO8_REG
    lw   t1, 0(t0)              /* leer valor actual */
    li   t2, ~(0x7 << 12)
    and  t1, t1, t2             /* borrar MCU_SEL */
    li   t2, (1 << 12)
    or   t1, t1, t2             /* MCU_SEL = 1 (funcion GPIO) */
    sw   t1, 0(t0)

    /* 2) Matriz GPIO: por el pin sale la senal 128 (salida GPIO simple)
     *    y el bit 9 (OEN_SEL) hace que la habilitacion venga de GPIO_ENABLE */
    li   t0, GPIO_FUNC8_OUT_SEL_CFG_REG
    li   t1, (128 | (1 << 9))
    sw   t1, 0(t0)

    /* 3) Dejar el pin en 1 (LED apagado) antes de habilitarlo */
    li   t1, LED_MASK
    li   t0, GPIO_OUT_W1TS_REG
    sw   t1, 0(t0)

    /* 4) Habilitar GPIO8 como salida */
    li   t0, GPIO_ENABLE_W1TS_REG
    sw   t1, 0(t0)
    ret
    .size led_iniciar, . - led_iniciar

/* void led_encender(void)
 * Pin en 0 -> LED prendido (activo en bajo). */
    .global led_encender
    .type   led_encender, @function
led_encender:
    li   t0, GPIO_OUT_W1TC_REG
    li   t1, LED_MASK
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
