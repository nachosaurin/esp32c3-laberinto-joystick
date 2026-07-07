import serial
import threading
import msvcrt
import time

puerto = "COM3"
baudios = 115200

ser = serial.Serial(puerto, baudios, timeout=0.1)

def leer_esp():
    while True:
        dato = ser.read(200)
        if dato:
            print(dato.decode(errors="ignore"), end="", flush=True)

hilo = threading.Thread(target=leer_esp, daemon=True)
hilo.start()

print("Teclas: W A S D para mover, R para reiniciar, Q para salir")

while True:
    if msvcrt.kbhit():
        tecla = msvcrt.getch()

        if tecla in [b'q', b'Q']:
            break

        ser.write(tecla + b"\n")

    time.sleep(0.02)

ser.close()