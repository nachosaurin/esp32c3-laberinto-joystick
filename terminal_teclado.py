import sys
import serial
import threading
import msvcrt
import time

# El puerto puede cambiar (COM3, COM4, COM5...) si conectas la placa en
# otro USB. Fijate cual es en el Administrador de dispositivos > Puertos.
# Uso: python terminal_teclado.py COM4
puerto = sys.argv[1] if len(sys.argv) > 1 else "COM3"
baudios = 115200

try:
    ser = serial.Serial(puerto, baudios, timeout=0.1)
except serial.SerialException as e:
    print(f"No se pudo abrir {puerto}: {e}")
    print("- Si dice 'Acceso denegado': cerra el monitor de ESP-IDF (o cualquier")
    print("  otro programa que este usando el puerto) y volve a intentar.")
    print("- Si dice que no existe: revisa en que COM quedo la placa y pasalo")
    print("  como argumento, por ejemplo: python terminal_teclado.py COM4")
    sys.exit(1)

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