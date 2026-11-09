#!/bin/bash
# Junta en un archivo la evidencia de terminal para el reporte del Avance 3:
# arranque, prioridades, drivers (clase 21), registros y tipos (clase 20) y la medición de latencia.
# Uso, en la Raspberry con los servicios corriendo:   sudo ./evidencia.sh
# Resultado: evidencia_<fecha>.log en la carpeta actual.
set -u
if [ "$(id -u)" -ne 0 ]; then echo "Ejecuta con sudo"; exit 1; fi
APP=/opt/cerradura
PY=$APP/venv/bin/python
SALIDA="evidencia_$(date +%F_%H%M).log"
exec > >(tee "$SALIDA") 2>&1

t() { echo; echo "===== $* ====="; }
c() { echo "\$ $*"; eval "$@"; echo; }

t "Sistema"
c "uname -a"
c "grep PRETTY_NAME /etc/os-release"
c "free -m"

t "Arranque (requisito Avance 1: operativo en menos de 60 s)"
c "systemd-analyze"
c "systemd-analyze blame | grep cerradura"
c "journalctl -b -o short-monotonic -u cerradura-control | grep -m1 'escuchando'"
c "journalctl -b -o short-monotonic -u cerradura-entradas | grep -m1 'RFID listo'"
c "journalctl -b -o short-monotonic -u cerradura-web | grep -m1 'Running on'"

t "Servicios y prioridades (bitácora clase 8)"
c "systemctl is-active cerradura-control cerradura-entradas cerradura-web"
c "ps -eo pid,ni,rss,args | grep -E 'control.py|entradas.py|web/app.py|rpicam-vid' | grep -v grep"
PID_CTL=$(pgrep -f "control.py" | head -1)
[ -n "$PID_CTL" ] && c "ps -L -o tid,ni,comm -p $PID_CTL"

t "IPC (bitácora clase 11): socket y cola de mensajes"
c "ls -l /tmp/cerradura_control.sock"
c "ls -l /dev/mqueue/ && cat /dev/mqueue/cerradura_puerta"

t "Clase 21: drivers del kernel y nodos en /dev"
c "lsmod | grep -E 'i2c|spi|bcm2835|unicam|ov5647|videodev' "
c "ls -l /dev/i2c-1 /dev/spidev0.0 /dev/gpiochip0 /dev/serial0 /dev/ttyAMA0 /dev/video0"
c "readlink -f /dev/serial0"
c "i2cdetect -y 1"
c "printf 'I2C1 a %d Hz\n' 0x\$(od -An -tx1 /sys/class/i2c-adapter/i2c-1/of_node/clock-frequency | tr -d ' \\n')"
c "gpioinfo gpiochip0 | grep -E 'line +(5|6|12|13|16|17|19|20|21):'"
c "dmesg | grep -iE 'ov5647|unicam|spi|ttyAMA|i2c' | tail -n 15"
c "rpicam-hello --list-cameras 2>&1 | head -n 8"

t "Clase 20: registros del PCA9685 (enteros de 8 bits por I2C)"
c "i2cget -y 1 0x40 0x00"
echo "PRE_SCALE (0xFE): debe ser 0x79 = 121 para 50 Hz"
c "i2cget -y 1 0x40 0xFE"
echo "Canal 15: LED15_OFF_L (0x44) y LED15_OFF_H (0x45); 205 = 0x00CD cerrado, 410 = 0x019A abierto"
c "i2cget -y 1 0x40 0x44; i2cget -y 1 0x40 0x45"

t "Clase 20: VersionReg del RC522 por SPI (lectura directa, sin reiniciar el chip)"
c "$PY -c \"import spidev; s=spidev.SpiDev(); s.open(0,0); s.max_speed_hz=1000000; print(hex(s.xfer2([0x80|(0x37<<1),0])[1]))\""

t "Clase 20: el pin del reed cambia sin que el programa lo escriba (equivalente a volatile)"
echo "Acerca y aleja el imán durante los próximos 6 s"
for i in 1 2 3 4 5 6; do pinctrl get 17; sleep 1; done

t "Medición: latencia credencial -> servo (requisito Avance 1: menos de 3 s)"
c "cd $APP && $PY admin.py medir"
c "cd $APP && $PY admin.py estado"

t "Base de datos"
c "ls -l /var/lib/cerradura"
c "$PY -c \"import sys; sys.path.insert(0,'$APP'); import db; c=db.conectar(); print(c.execute('select count(*) from logs').fetchone()[0], 'eventos'); print(c.execute('select count(*) from usuarios').fetchone()[0], 'usuarios')\""

echo "Evidencia guardada en $SALIDA"
