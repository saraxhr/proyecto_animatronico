import serial
import time
from Adafruit_IO import Client

# ========== CONFIGURACIÓN ==========
ADAFRUIT_IO_USERNAME = ""
ADAFRUIT_IO_KEY = ""
SERIAL_PORT = 'COM4'  
BAUD_RATE = 9600
# ===================================

# Feeds de los sliders
servo_feeds = ['servo1', 'servo2', 'servo3', 'servo4']
ultimo_estado = [None] * 4

# Conexión a Adafruit IO
aio = Client(ADAFRUIT_IO_USERNAME, ADAFRUIT_IO_KEY)

# Conexión UART con el microcontrolador
try:
    uart = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    uart.flushInput()
    uart.flushOutput()
    time.sleep(2)
    print(f"Conectado a {SERIAL_PORT} @ {BAUD_RATE} baudios")
except Exception as e:
    print(f"No se pudo abrir el puerto serial: {e}")
    exit(1)

print("Iniciando transmisión UART desde Adafruit IO")

while True:
    try:
        # Leer valores actuales de los sliders
        valores = []
        for feed in servo_feeds:
            valor = int(aio.receive(feed).value)
            valores.append(valor)

        # Solo enviar si cambia el valor
        if valores != ultimo_estado:
            # Construir comando
            comando = ""
            for i in range(4):
                comando += f"servo{i+1}:{valores[i]};"
            comando += "\n"

            # Enviar comando por UART
            uart.write(comando.encode('utf-8'))
            print(f"Enviado: {comando.strip()}")

            # Leer respuesta
            time.sleep(0.2)  # Aumentar espera para respuesta
            while uart.in_waiting > 0:
                respuesta = uart.read(uart.in_waiting).decode('utf-8', errors='ignore')
                print(f"Recibido: {respuesta.strip()}")

            ultimo_estado = valores[:]

        time.sleep(1)  # No saturar Adafruit IO

    except serial.SerialException as se:
        print(f"Error serial: {se}")
        uart.close()
        try:
            uart = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
            print("Reconectado al puerto serial")
        except:
            print("No se pudo reconectar, esperando...")
            time.sleep(5)
    except Exception as error:
        print(f"Error: {error}")
        time.sleep(2)