#!/bin/bash
# Instala la cerradura en una Raspberry Pi 3B con Raspberry Pi OS (Bookworm).
# Uso, desde la carpeta cerradura del repo:   sudo ./instalar.sh
# Se puede volver a correr: solo agrega lo que falta y actualiza el código.
set -euo pipefail
cd "$(dirname "$0")"

if [ "$(id -u)" -ne 0 ]; then echo "Ejecuta con sudo"; exit 1; fi

# 1) Interfaces: I2C (PCA9685, LCD), SPI (RC522), UART libre para la huella, cámara
CFG=/boot/firmware/config.txt; [ -f "$CFG" ] || CFG=/boot/config.txt
CMD=/boot/firmware/cmdline.txt; [ -f "$CMD" ] || CMD=/boot/cmdline.txt
for l in "dtparam=i2c_arm=on" "dtparam=spi=on" "enable_uart=1" "dtoverlay=disable-bt" "camera_auto_detect=1"; do
  grep -qxF "$l" "$CFG" || echo "$l" >> "$CFG"
done
systemctl disable --now hciuart 2>/dev/null || true          # el Bluetooth ya no usa el UART
sed -i 's/console=serial0,[0-9]* //' "$CMD"                    # la consola tampoco

# 2) Paquetes del sistema
apt-get update
apt-get install -y python3-venv python3-dev i2c-tools gpiod swig liblgpio-dev openssl
apt-get install -y rpicam-apps-lite || apt-get install -y libcamera-apps-lite || \
  echo "AVISO: instala rpicam-apps para la cámara"

# 3) Código y entorno de Python
mkdir -p /opt/cerradura /var/lib/cerradura
chmod 700 /var/lib/cerradura
cp -r ./*.py hw web /opt/cerradura/
[ -d /opt/cerradura/venv ] || python3 -m venv /opt/cerradura/venv
/opt/cerradura/venv/bin/pip install -r requirements.txt

# HTTPS: certificado autofirmado (10 años) para la app web; se genera una sola vez
if [ ! -f /var/lib/cerradura/https.crt ]; then
  IP=$(hostname -I | awk '{print $1}')
  openssl req -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=cerradura" \
    -addext "subjectAltName=IP:${IP:-127.0.0.1},DNS:$(hostname).local,DNS:localhost" \
    -keyout /var/lib/cerradura/https.key -out /var/lib/cerradura/https.crt
  chmod 600 /var/lib/cerradura/https.key
fi

# 4) Servicios: arrancan solos al encender
cp systemd/*.service systemd/*.timer /etc/systemd/system/
systemctl daemon-reload
systemctl enable cerradura-control cerradura-entradas cerradura-web
systemctl enable --now cerradura-respaldo.timer             # respaldo diario de la base

# 5) Memoria USB para respaldos (opcional): formatearla con la etiqueta CERRADURA.
#    nofail: si no está conectada, la Pi arranca igual y el respaldo queda solo en la tarjeta.
mkdir -p /mnt/respaldo
grep -q "LABEL=CERRADURA" /etc/fstab || \
  echo "LABEL=CERRADURA /mnt/respaldo auto defaults,nofail,noatime,x-systemd.device-timeout=5s 0 2" >> /etc/fstab

echo
echo "Listo. Falta:"
echo "  1) sudo reboot"
echo "  2) cd /opt/cerradura && sudo venv/bin/python admin.py web-admin <usuario>"
echo "  3) Abrir https://$(hostname -I | awk '{print $1}'):8000 desde la red local (aceptar el aviso del certificado)"
echo "  4) (Opcional) Conectar una memoria USB con etiqueta CERRADURA para los respaldos"