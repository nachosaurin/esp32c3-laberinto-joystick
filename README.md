# Laberinto en consola con ESP32-C3 + joystick

Proyecto ESP-IDF (v5.2) para ESP32-C3. El laberinto se dibuja en la consola
serie y el jugador (`P`) se mueve con un joystick analogico hasta llegar a la
meta (`M`). El boton del joystick (o la tecla R en el monitor) reinicia la partida.

## Conexion del joystick (KY-023)

| Joystick | ESP32-C3 |
| -------- | -------- |
| GND      | GND      |
| +5V/VCC  | **3V3** (no 5 V) |
| VRx      | GPIO0 (ADC1 canal 0) |
| VRy      | GPIO1 (ADC1 canal 1) |
| SW       | GPIO3 (reiniciar) |

Si usas otros pines, cambia los `#define JOY_...` al principio de `main/main.c`.
En la ESP32-C3 conviene usar GPIO0 a GPIO4 para los ejes (son los del ADC1).

## Compilar y cargar

1. Abrir la carpeta con VS Code + extension ESP-IDF (o la terminal "ESP-IDF 5.2 CMD").
2. Elegir el target `esp32c3` y el puerto COM correcto.
3. `idf.py build flash monitor`
