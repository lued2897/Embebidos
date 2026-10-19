#ifndef HW_PI_H
#define HW_PI_H

#include <stdio.h>
#include "centinela_cfg.h"
#include "gpio_regs.h"
#include "sensores_lib.h"

typedef struct {
	gpio_regs_t *gpio;
	int fd_i2c;
	int fd_spi;
	struct bmp_calib calib;
} hw_t;

static const unsigned hw_pines_led[4] = {LED_1, LED_2, LED_3, LED_4};
static const unsigned hw_pines_bus[6] = {2, 3, 8, 9, 10, 11};

static void hw_funcion(gpio_regs_t *gpio, unsigned pin, uint32_t funcion)
{
	unsigned reg = pin / 10;
	unsigned desp = (pin % 10) * 3;
	uint32_t valor = gpio->GPFSEL[reg];

	valor &= ~(0x7u << desp);
	valor |= (funcion & 0x7u) << desp;
	gpio->GPFSEL[reg] = valor;
}

static uint32_t hw_campo(gpio_regs_t *gpio, unsigned pin)
{
	return (gpio->GPFSEL[pin / 10] >> ((pin % 10) * 3)) & 0x7u;
}

static int hw_abrir(hw_t *h, int direccion_bmp)
{
	uint8_t modo = SPI_MODE_0;

	h->gpio = gpio_abrir();
	h->fd_i2c = open("/dev/i2c-1", O_RDWR);
	h->fd_spi = open("/dev/spidev0.0", O_RDWR);
	if (h->gpio == NULL || h->fd_i2c < 0 || h->fd_spi < 0) {
		perror("open");
		return -1;
	}
	if (ioctl(h->fd_i2c, I2C_SLAVE, direccion_bmp) < 0) {
		perror("ioctl I2C_SLAVE");
		return -1;
	}
	if (ioctl(h->fd_spi, SPI_IOC_WR_MODE, &modo) < 0) {
		perror("ioctl SPI_IOC_WR_MODE");
		return -1;
	}
	if (bmp_calibracion(h->fd_i2c, &h->calib) < 0 || bmp_iniciar(h->fd_i2c) < 0) {
		fprintf(stderr, "No responde el BMP280 en 0x%02X\n", direccion_bmp);
		return -1;
	}
	for (int i = 0; i < 4; i++)
		hw_funcion(h->gpio, hw_pines_led[i], 1);
	hw_funcion(h->gpio, BOTON_PAUSA, 0);
	hw_funcion(h->gpio, BOTON_ESTADO, 0);
	return 0;
}

static int hw_temp(hw_t *h, int32_t *temp_c100)
{
	uint8_t t[3];

	if (bmp_leer_registros(h->fd_i2c, BMP_REG_TEMP, t, sizeof(t)) < 0)
		return -1;
	*temp_c100 = bmp_compensar(&h->calib, bmp_temperatura_cruda(t));
	return 0;
}

static int hw_pot(hw_t *h, unsigned *valor)
{
	return pot_leer(h->fd_spi, 0, valor);
}

static unsigned hw_botones(hw_t *h)
{
	uint32_t lev = h->gpio->GPLEV[0];

	return ((lev >> BOTON_PAUSA) & 1u) | (((lev >> BOTON_ESTADO) & 1u) << 1);
}

static void hw_leds(hw_t *h, uint32_t mascara)
{
	h->gpio->GPSET[0] = mascara & LED_MASCARA;
	h->gpio->GPCLR[0] = LED_MASCARA & ~mascara;
}

static void hw_cerrar(hw_t *h)
{
	h->gpio->GPCLR[0] = LED_MASCARA;
	for (int i = 0; i < 4; i++)
		hw_funcion(h->gpio, hw_pines_led[i], 0);
	for (int i = 0; i < 6; i++)
		if (hw_campo(h->gpio, hw_pines_bus[i]) != 4)
			fprintf(stderr, "Aviso: GPIO%u ya no esta en ALT0\n", hw_pines_bus[i]);
}

#endif
