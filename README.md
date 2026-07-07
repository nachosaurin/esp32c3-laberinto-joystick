# Proyecto 24 - Laberinto con joystick en ESP32-C3

Mini-juego para ESP-IDF donde un jugador se mueve dentro de un laberinto fijo
usando un joystick analogico HW-504. La terminal UART/USB muestra el estado del
juego en texto.

## Estructura

```text
maze_project/
|-- CMakeLists.txt
|-- sdkconfig.defaults
`-- main/
    |-- CMakeLists.txt
    |-- main.c
    |-- maze.c
    |-- maze.h
    |-- joystick.c
    |-- joystick.h
    |-- uart_view.c
    |-- uart_view.h
    |-- asm_functions.h
    |-- asm_collision.S
    `-- asm_joystick.S
```

## Compilacion y ejecucion

Desde una terminal con ESP-IDF cargado:

```powershell
cd maze_project
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor
```

Reemplazar `COMx` por el puerto serie de la placa.

## Conexion del joystick HW-504

| HW-504 | ESP32-C3 |
| ------ | -------- |
| VCC    | 3V3      |
| GND    | GND      |
| VRx    | GPIO del canal ADC configurado como `JOYSTICK_X_ADC_CHANNEL` |
| VRy    | GPIO del canal ADC configurado como `JOYSTICK_Y_ADC_CHANNEL` |
| SW     | Sin usar |

Los canales se cambian en `main/joystick.c`. Por defecto se usan
`ADC_CHANNEL_0` y `ADC_CHANNEL_1` de `ADC_UNIT_1`, pensados para ESP32-C3.

## Distribucion entre C y assembler

C se usa para la logica general, inicializacion de perifericos, lectura ADC,
estado del juego, render por terminal y reinicio. Esto mejora legibilidad,
mantenimiento y permite trabajar con APIs de ESP-IDF.

Assembler RISC-V se usa en dos funciones puntuales:

- `asm_check_cell`: valida limites, calcula `offset = y * width + x`, carga la
  celda con `lbu` como byte sin signo y devuelve si esta bloqueada, libre o es
  meta.
- `asm_process_joystick`: calcula `dx` y `dy`, aplica zona muerta y elige el eje
  predominante para devolver una direccion cardinal sin diagonales.

## Flujo de juego

1. Se inicializa la terminal y el ADC.
2. Se calibra el centro del joystick promediando varias lecturas.
3. Se reinicia el estado: posicion inicial, contador en cero y juego activo.
4. El bucle principal lee el joystick y llama a `asm_process_joystick`.
5. Si hay direccion, se calcula la celda destino y `asm_check_cell` decide si
   se puede avanzar.
6. Si la celda es pared, se informa `Movimiento bloqueado`, sin mover al jugador
   ni incrementar movimientos.
7. Si la celda es libre o meta, se actualiza la posicion, se incrementa el
   contador y se redibuja el laberinto.
8. Al llegar a la meta se muestra `Laberinto completado`, se informa el total de
   movimientos validos y se dejan de procesar movimientos.
9. Con `R` o `r` desde la terminal se reinicia la partida.

## Prueba sin joystick

Para probar la placa sin conectar el HW-504, el proyecto incluye un modo de
teclado por UART activado en `main/main.c` con:

```c
#define UART_KEYBOARD_TEST_MODE 1
```

Con el monitor serie abierto:

- `W` mueve arriba.
- `S` mueve abajo.
- `A` mueve izquierda.
- `D` mueve derecha.
- `R` reinicia la partida.

Para volver a usar solo el joystick en una demostracion con hardware, cambiar
`UART_KEYBOARD_TEST_MODE` a `0`, compilar y flashear nuevamente.

## Interfaz grafica en PC

Ademas del monitor serie, el proyecto incluye una interfaz grafica opcional en
Python/Tkinter que se conecta al mismo puerto serie de la ESP32-C3 y dibuja el
laberinto como una grilla de colores.

Antes de abrirla, cerrar cualquier `idf.py monitor` activo, porque solo un
programa puede usar `COM7` a la vez.

Desde PowerShell:

```powershell
cd "C:\Users\maxim\OneDrive\Documentos\New project\maze_project"
.\maze_gui.cmd
```

Si la placa aparece en otro puerto:

```powershell
.\maze_gui.cmd --port COM8
```

La app permite mover con los botones o con el teclado:

- `W`: arriba.
- `A`: izquierda.
- `S`: abajo.
- `D`: derecha.
- `R`: reiniciar.

La logica del juego sigue corriendo en el ESP32-C3; la aplicacion de PC solo
manda comandos por UART y representa graficamente el estado recibido.

## Puntos para la defensa oral

- El ESP32-C3 usa arquitectura RISC-V, por eso los archivos `.S` contienen
  assembler RISC-V y respetan la ABI con argumentos en `a0` a `a4`.
- La matriz bidimensional de C se guarda de forma lineal por filas. El assembler
  reproduce el acceso a `maze[y][x]` con `y * width + x`.
- `lbu` es importante porque las celdas son `uint8_t` y deben leerse sin signo.
- La zona muerta evita falsos movimientos por ruido analogico.
- El cooldown evita que una inclinacion sostenida genere demasiados pasos por
  segundo.
- El reinicio por UART permite demostrar el juego varias veces sin reflashear.
