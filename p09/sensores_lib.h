#ifndef SENSORES_LIB_H
#define SENSORES_LIB_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
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

#define DS3231_DIR         0x68
#define DS3231_REG_TEMP    0x11   /* 0x11: entero (C2), 0x12: bits 7-6 = 0.25 C */
#define DS3231_REG_CONTROL 0x0E
#define DS3231_REG_STATUS  0x0F
#define DS3231_CONV        0x20u  /* control, bit 5: forzar conversion */
#define DS3231_BSY         0x04u  /* estado, bit 2: ocupado */

static int ds3231_leer_registros(int fd, uint8_t reg, uint8_t *dst, size_t n)
{
	if (write(fd, &reg, 1) != 1)
		return -1;
	if (read(fd, dst, n) != (ssize_t)n)
		return -1;
	return 0;
}

static int ds3231_escribir_registro(int fd, uint8_t reg, uint8_t valor)
{
	uint8_t b[2] = {reg, valor};

	return write(fd, b, 2) == 2 ? 0 : -1;
}

/* El DS3231 convierte solo cada 64 s. Esto pide una conversion nueva
 * (unos 200 ms) si no hay una en curso. */
static int ds3231_convertir(int fd)
{
	uint8_t control, estado;

	if (ds3231_leer_registros(fd, DS3231_REG_CONTROL, &control, 1) < 0 ||
	    ds3231_leer_registros(fd, DS3231_REG_STATUS, &estado, 1) < 0)
		return -1;
	if ((control & DS3231_CONV) || (estado & DS3231_BSY))
		return 0;
	return ds3231_escribir_registro(fd, DS3231_REG_CONTROL, (uint8_t)(control | DS3231_CONV));
}

/* Comprueba que responde y deja lista la primera lectura */
static int ds3231_iniciar(int fd)
{
	uint8_t control;

	if (ds3231_convertir(fd) < 0)
		return -1;
	for (int i = 0; i < 30; i++) {
		usleep(10000);
		if (ds3231_leer_registros(fd, DS3231_REG_CONTROL, &control, 1) < 0)
			return -1;
		if (!(control & DS3231_CONV))
			break;
	}
	return 0;
}

/* Temperatura en centesimas de grado Celsius (pasos de 25) */
static int ds3231_leer_temp_c100(int fd, int32_t *temp_c100)
{
	uint8_t t[2];
	int32_t cuartos;

	if (ds3231_leer_registros(fd, DS3231_REG_TEMP, t, sizeof(t)) < 0)
		return -1;
	cuartos = (int32_t)(int8_t)t[0] * 4 + (int32_t)(t[1] >> 6);
	*temp_c100 = cuartos * 25;
	(void)ds3231_convertir(fd);   /* prepara la siguiente lectura */
	return 0;
}

#endif
