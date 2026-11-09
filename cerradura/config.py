"""Configuración central del sistema de cerradura (Equipo 8 - FSE)."""
import os

# ---------------- Rutas ----------------
DATA_DIR = os.environ.get("CERRADURA_DATA", "/var/lib/cerradura")
DB_PATH = os.path.join(DATA_DIR, "cerradura.db")
PEPPER_PATH = os.path.join(DATA_DIR, "pepper.key")   # llave secreta para el hash de PINs
WEB_SECRET_PATH = os.path.join(DATA_DIR, "web.key")  # llave para firmar las sesiones web

# ---------------- IPC (bitácora clase 11) ----------------
SOCKET_PATH = "/tmp/cerradura_control.sock"           # entradas/web -> control (FIFO, una solicitud a la vez)
MQ_PUERTA = "/cerradura_puerta"                       # cola de mensajes POSIX: estado del reed switch

# ---------------- GPIO (numeración BCM) ----------------
# Teclado matricial 4x4
TECLADO_FILAS = [5, 6, 13, 19]       # salidas
TECLADO_COLS = [12, 16, 20, 21]      # entradas con pull-up
TECLADO_MAPA = [
    ["1", "2", "3", "A"],
    ["4", "5", "6", "B"],
    ["7", "8", "9", "C"],
    ["*", "0", "#", "D"],
]
PIN_MIN, PIN_MAX = 4, 6
PIN_TIMEOUT_S = 10                   # se borra lo tecleado tras 10 s sin teclas

# Reed switch MC-38 (un cable a GPIO17, el otro a GND)
REED_GPIO = 17

# ---------------- UART: huella AS608 ----------------
HUELLA_ACTIVA = True                
HUELLA_PUERTO = "/dev/serial0"
HUELLA_BAUD = 57600

# ---------------- SPI: RFID RC522 ----------------
RFID_SPI_BUS = 0
RFID_SPI_DEV = 0                     # CE0
RFID_COOLDOWN_S = 3                  # ignora la misma tarjeta durante 3 s
RFID_SALUD_S = 2                     # cada 2 s confirma que el lector sigue respondiendo

# ---------------- I2C ----------------
I2C_BUS = 1
PCA9685_ADDR = 0x40
SERVO_CANAL = 15
# PWM 12 bits a 50 Hz: 20 ms / 4096 cuentas (bitácora clase 12)
SERVO_CERRAR = 205                   # 1.0 ms
SERVO_ABRIR = 410                    # 2.0 ms

LCD_ADDR = 0x27
LCD_COLS, LCD_FILAS = 20, 4

# ---------------- Prioridades (bitácora clase 8) ----------------
# Valor nice: más bajo = más prioridad. Los de proceso se fijan en los servicios de systemd.
NICE_CONTROL = -10                   # servo y decisión de apertura: prioridad alta
NICE_ENTRADAS = -5                   # lectura de sensores: alta
NICE_WEB = 0                         # app web: normal
NICE_CAMARA = 10                     # cámara: media-baja, no debe retrasar a la cerradura
NICE_HILO_LCD = 10                   # desplazamiento del display: baja

# ---------------- Lógica de control ----------------
TIEMPO_ABIERTO_S = 8                 # si nadie abre la puerta, se vuelve a cerrar
MAX_INTENTOS = 5                     # intentos fallidos seguidos antes del bloqueo
BLOQUEO_S = 30

# ---------------- App web ----------------
WEB_PUERTO = 8000
WEB_SESION_MIN = 10                  # la sesión caduca tras 10 min sin actividad (requisito Avance 1)
WEB_MAX_INTENTOS = 5                 # intentos de login fallidos antes de bloquear
WEB_BLOQUEO_S = 60
WEB_ESTADO_MS = 500                  # la página consulta el estado de la puerta cada 500 ms (< 1 s)

# ---------------- Cámara (pipe rpicam-vid -> web, bitácora clase 9) ----------------
CAMARA_ACTIVA = True
CAMARA_ANCHO, CAMARA_ALTO = 640, 480
CAMARA_FPS = 15
