#ifndef SENSORES_LIB_H
#define SENSORES_LIB_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>

/* ---------- MCP3008 por SPI ---------- */

static unsigned mcp3008_valor(const uint8_t rx[3])
{
	return ((unsigned)(rx[1] & 0x03u) << 8) | rx[2];
}

static int pot_leer(int fd_spi, unsigned canal, unsigned *valor)
{
	uint8_t tx[3] = {0x01, (uint8_t)((8u + canal) << 4), 0x00};
	uint8_t rx[3] = {0, 0, 0};
	struct spi_ioc_transfer tr;

	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 3;
	tr.speed_hz = 1000000;
	tr.bits_per_word = 8;

	if (ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr) < 0)
		return -1;

	*valor = mcp3008_valor(rx);
	return 0;
}

/* ---------- DS3231 por I2C ---------- */

#define DS3231_REG_TEMP 0x11

/* 
 * Lee la temperatura del RTC DS3231.
 * El registro 0x11 tiene la parte entera (complemento a 2).
 * El registro 0x12 tiene la fracción en los 2 bits más significativos (pasos de 0.25 °C).
 */
static int ds3231_leer_temperatura(int fd, float *temperatura)
{
	uint8_t reg = DS3231_REG_TEMP;
	uint8_t buf[2];

	/* Se apunta al registro de temperatura MSB */
	if (write(fd, &reg, 1) != 1)
		return -1;

	/* Se leen 2 bytes: MSB (0x11) y LSB (0x12) */
	if (read(fd, buf, 2) != 2)
		return -1;

	/* El casteo a int8_t mantiene el signo si la temperatura es negativa */
	int8_t parte_entera = (int8_t)buf[0];
	
	/* Desplazar 6 bits a la derecha para obtener valores de 0 a 3 (x 0.25) */
	uint8_t parte_fraccional = buf[1] >> 6;

	*temperatura = (float)parte_entera + ((float)parte_fraccional * 0.25f);
	
	return 0;
}

#endif