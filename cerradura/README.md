# Cerradura multifactor — Módulo de entradas (Avance)

Raspberry Pi 3B · Raspberry Pi OS · Python 3

```
 Huella AS608 ──UART──┐
 RFID RC522 ───SPI────┤                       socket UNIX (FIFO)
 Teclado 4x4 ──GPIO───┼─► entradas.py ───────────────────────────► control.py ──I2C──► PCA9685 ──► Servo
 Reed MC-38 ───GPIO───┘        │                                     │   │
                               └──── cola de mensajes POSIX ─────────┘   ├──I2C──► LCD 20x4
                                     (estado de la puerta)               └──► SQLite (usuarios + logs)
```

| Archivo | Función |
|---|---|
| `entradas.py` | Un hilo por sensor: huella, RFID, teclado, reed switch |
| `control.py` | Autentica, abre/cierra el servo, actualiza el LCD, registra logs, bloqueo por intentos |
| `admin.py` | Alta/baja de usuarios, registro de PIN, tarjeta y huella; consulta de logs |
| `db.py` | SQLite (WAL + synchronous=FULL), PINs con HMAC-SHA256 |
| `ipc.py` | Socket UNIX y cola de mensajes |
| `hw/` | Drivers: `pca9685.py`, `lcd.py` (PCF8574), `rc522.py` |
| `config.py` | Pines, direcciones I2C, tiempos |

## Conexiones

| Módulo | Pin del módulo | Raspberry Pi (pin físico) |
|---|---|---|
| **AS608** | VCC / GND | 3.3V (1) / GND (6) |
| | TX / RX | GPIO15 RXD (10) / GPIO14 TXD (8) |
| **RC522** | 3.3V / GND | 3.3V (17) / GND (20) |
| | SDA(SS) / SCK | GPIO8 CE0 (24) / GPIO11 (23) |
| | MOSI / MISO | GPIO10 (19) / GPIO9 (21) |
| | RST | 3.3V |
| **Teclado 4x4** | F1 F2 F3 F4 | GPIO5 (29), GPIO6 (31), GPIO13 (33), GPIO19 (35) |
| | C1 C2 C3 C4 | GPIO12 (32), GPIO16 (36), GPIO20 (38), GPIO21 (40) |
| **Reed MC-38** | 2 cables | GPIO17 (11) y GND (9) |
| **PCA9685** | VCC / GND | 3.3V (1) / GND (14) |
| | SDA / SCL | GPIO2 (3) / GPIO3 (5) |
| | V+ (servo) | **Fuente externa 5V** (GND común con la Pi) |
| | Canal 0 | Señal del servo |
| **LCD 2004 + PCF8574** | VCC / GND | 5V (2) / GND (25) |
| | SDA / SCL | GPIO2 (3) / GPIO3 (5) |

> El servo NO se alimenta de la Pi (evita reinicios por caída de voltaje).

## Instalación

```bash
git clone https://github.com/lued2897/Embebidos && cd Embebidos/cerradura
sudo ./instalar.sh
sudo reboot
i2cdetect -y 1  
```

Si el LCD sale en `3f`, cambia `LCD_ADDR = 0x3F` en `config.py`.

## Registrar usuarios

```bash
cd /opt/cerradura
sudo systemctl stop cerradura-entradas
sudo venv/bin/python admin.py agregar claudio
sudo venv/bin/python admin.py pin claudio
sudo venv/bin/python admin.py tarjeta claudio
sudo venv/bin/python admin.py huella claudio
sudo venv/bin/python admin.py listar
sudo systemctl start cerradura-entradas
```

## Uso / pruebas

```bash
sudo venv/bin/python admin.py logs 20      # historial
sudo venv/bin/python admin.py abrir        # prueba del servo
sudo venv/bin/python admin.py estado       # puerta abierta/cerrada, bloqueo
journalctl -fu cerradura-control           # ver eventos en vivo
```

Teclado: dígitos → PIN, `#` → enviar, `*` → borrar.

### Sin hardware (simulación)

```bash
CERRADURA_DATA=./datos python3 control.py --sim      # terminal 1
CERRADURA_DATA=./datos python3 entradas.py --sim     # terminal 2
> rfid A1B2C3D4
> pin 1234
> puerta abierta
```
