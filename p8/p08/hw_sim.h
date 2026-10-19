#ifndef HW_SIM_H
#define HW_SIM_H

/* Circuito simulado: la temperatura sube y baja sola y se "presionan" los botones
 * a tiempos fijos. Sirve para probar el programa sin Raspberry Pi. */

#include <stdio.h>
#include <time.h>
#include "centinela_cfg.h"

typedef struct {
	uint32_t t0_ms;
	uint32_t leds;
} hw_t;

static uint32_t hw_sim_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static int hw_abrir(hw_t *h, int direccion_bmp)
{
	(void)direccion_bmp;
	h->t0_ms = hw_sim_ms();
	h->leds = 0;
	printf("[simulado] circuito simulado\n");
	return 0;
}

/* 25.00 C hasta 3 s; sube 1.50 C por segundo hasta 28.00 C (5 s); se queda hasta 8 s;
 * baja 1.00 C por segundo hasta 25.00 C (11 s) */
static int hw_temp(hw_t *h, int32_t *temp_c100)
{
	int32_t t = (int32_t)(hw_sim_ms() - h->t0_ms);
	int32_t c = 2500;

	if (t >= 3000 && t < 5000)
		c = 2500 + (t - 3000) * 150 / 1000;
	else if (t >= 5000 && t < 8000)
		c = 2800;
	else if (t >= 8000 && t < 11000)
		c = 2800 - (t - 8000) * 100 / 1000;
	*temp_c100 = c;
	return 0;
}

static int hw_pot(hw_t *h, unsigned *valor)
{
	(void)h;
	*valor = 512;
	return 0;
}

/* boton de pausa presionado de 12.0 a 12.3 s y de 14.0 a 14.3 s; boton de estado de 6.0 a 6.3 s */
static unsigned hw_botones(hw_t *h)
{
	uint32_t t = hw_sim_ms() - h->t0_ms;
	unsigned b = 0;

	if ((t >= 12000 && t < 12300) || (t >= 14000 && t < 14300))
		b |= 1u;
	if (t >= 6000 && t < 6300)
		b |= 2u;
	return b;
}

static void hw_leds(hw_t *h, uint32_t mascara)
{
	h->leds = mascara & LED_MASCARA;
	printf("[simulado] LEDs: %c%c%c%c\n",
	       (h->leds & BIT(LED_1)) ? '#' : '.', (h->leds & BIT(LED_2)) ? '#' : '.',
	       (h->leds & BIT(LED_3)) ? '#' : '.', (h->leds & BIT(LED_4)) ? '#' : '.');
}

static void hw_cerrar(hw_t *h)
{
	hw_leds(h, 0);
}

#endif
