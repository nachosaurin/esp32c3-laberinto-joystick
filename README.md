# Laberinto en consola con ESP32-C3 + joystick

Proyecto ESP-IDF (v5.2) para ESP32-C3. El laberinto se dibuja en la consola
serie y el jugador (`P`) se mueve con un joystick analogico hasta llegar a la
meta (`M`). Hundí el joystick o presioná `R` en el monitor para reiniciar.

## Conexion del joystick (KY-023)

| Joystick | ESP32-C3 |
| -------- | -------- |
| GND      | GND      |
| +5V/VCC  | **3V3** (no 5 V) |
| VRx      | GPIO0 (ADC1 canal 0) |
| VRy      | GPIO1 (ADC1 canal 1) |
| SW       | GPIO3 (botón de reinicio) |

Con el USB desconectado, agregá el cable entre `SW` y `GPIO3`. Si dejás
`SW` sin conectar, podés seguir reiniciando con `R`.
Si usas otros pines, cambia los `#define JOY_...` al principio de `main/main.c`.
En la ESP32-C3 conviene usar GPIO0 a GPIO4 para los ejes (son los del ADC1).

## LED de la placa (assembler)

`main/led.S` maneja el LED de la placa (GPIO8 en la ESP32-C3 Super Mini, activo en bajo)
escribiendo directo en los registros del GPIO. Parpadea con cada "Movimiento bloqueado"
y queda prendido al completar el laberinto. La logica del juego y el ADC siguen en C.

## Compilar y cargar

1. Abrir la carpeta con VS Code + extension ESP-IDF (o la terminal "ESP-IDF 5.2 CMD").
2. Elegir el target `esp32c3` y el puerto COM correcto.
3. `idf.py build flash monitor`

## Diagnóstico por consola serie

Para ver los reinicios y los intentos de movimiento, cambiá `DEBUG_LABERINTO`
de `0` a `1` en `main/main.c`, recompilá y cargá el programa. El monitor
muestra el origen y el destino de cada intento junto con el total de
movimientos válidos. El tablero se imprime debajo del anterior para
conservar las trazas en el historial.

Ejemplo ilustrativo desde la posición inicial, inclinando primero a la
derecha y después hacia arriba:

```text
[debug] Movimiento valido. (1,1) -> (2,1) validos=1
[debug] Movimiento bloqueado por pared. (2,1) -> (2,0) validos=1
```

Con `DEBUG_LABERINTO 0` se desactivan las trazas y se recupera el redibujado
normal del tablero.
