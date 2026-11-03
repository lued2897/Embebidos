#!/bin/bash
set -e

# 1) Interfaces: I2C, SPI, UART
CFG=/boot/firmware/config.txt; [ -f $CFG ] || CFG=/boot/config.txt
for l in "dtparam=i2c_arm=on" "dtparam=spi=on" "enable_uart=1" "dtoverlay=disable-bt"; do
  grep -qxF "$l" $CFG || echo "$l" >> $CFG
done
systemctl disable --now hciuart 2>/dev/null || true
sed -i 's/console=serial0,[0-9]* //' /boot/firmware/cmdline.txt 2>/dev/null || sed -i 's/console=serial0,[0-9]* //' /boot/cmdline.txt

# 2) Dependencias
apt-get update
apt-get install -y python3-venv python3-dev i2c-tools swig liblgpio-dev
mkdir -p /opt/cerradura /var/lib/cerradura
cp -r ./*.py hw /opt/cerradura/
python3 -m venv /opt/cerradura/venv
/opt/cerradura/venv/bin/pip install -r requirements.txt

# 3) Servicios
cp systemd/*.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable cerradura-control cerradura-entradas

echo "Listo. Reinicia la Raspberry: sudo reboot"
