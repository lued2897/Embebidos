#ifndef CENTINELA_LOGICA_H
#define CENTINELA_LOGICA_H

/* Logica del Centinela. Aqui se escribe el trabajo de la Parte B.
 * Las funciones no usan hardware: se prueban con `make prueba`.
 * Cada funcion trae su descripcion; las reglas numericas estan en centinela_cfg.h.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "centinela_cfg.h"

/* Promedio entero (division entera) de n lecturas de temperatura en centesimas
 * de grado. Debe funcionar con valores negativos y no desbordar la suma. */
static int32_t temp_base(const int32_t *lecturas, size_t n)
{
	int64_t suma = 0;
	size_t i;

	if (lecturas == NULL || n == 0)
		return 0;
	for (i = 0; i < n; i++)
		suma += (int64_t)lecturas[i];
	return (int32_t)(suma / (int64_t)n);
}

/* Umbral de alarma en centesimas: base + UMBRAL_MIN_C100 + pot * UMBRAL_RANGO_C100 / 1023.
 * pot va de 0 a 1023. Solo aritmetica entera. */
static int32_t umbral_desde_pot(int32_t base_c100, unsigned pot)
{
	int64_t parte = (int64_t)pot * (int64_t)UMBRAL_RANGO_C100 / 1023;
	return base_c100 + (int32_t)UMBRAL_MIN_C100 + (int32_t)parte;
}

/* LEDs encendidos segun el margen = umbral - temperatura (en centesimas):
 *   margen <= 0   -> 4
 *   margen <= 100 -> 3
 *   margen <= 200 -> 2
 *   margen <= 400 -> 1
 *   mayor         -> 0 */
static unsigned nivel_barra(int32_t margen_c100)
{
	if (margen_c100 <= 0)
		return 4u;
	if (margen_c100 <= 100)
		return 3u;
	if (margen_c100 <= 200)
		return 2u;
	if (margen_c100 <= 400)
		return 1u;
	return 0u;
}

/* Mascara de bits de los primeros `nivel` LEDs en el orden LED_1, LED_2, LED_3, LED_4.
 * Si nivel es mayor que 4, enciende los cuatro. */
static uint32_t mascara_leds(unsigned nivel)
{
	const uint32_t leds[4] = {
		BIT(LED_1), BIT(LED_2),
		BIT(LED_3), BIT(LED_4)
	};
	uint32_t m = 0;
	unsigned i;

	if (nivel > 4u)
		nivel = 4u;
	for (i = 0; i < nivel; i++)
		m |= leds[i];
	return m;
}

/* Patron de ALARMA: en las fases pares encienden LED_1 y LED_3; en las impares,
 * LED_2 y LED_4. */
static uint32_t mascara_alarma(unsigned fase)
{
	if ((fase % 2u) == 0u)
		return BIT(LED_1) | BIT(LED_3);
	return BIT(LED_2) | BIT(LED_4);
}

/* Maquina de estados del Centinela.
 *   PAUSADO:   solo sale si hay_pausa (pasa a VIGILANDO). Ignora la temperatura.
 *   VIGILANDO: con hay_pausa pasa a PAUSADO; si temp >= umbral pasa a ALARMA.
 *   ALARMA:    con hay_pausa pasa a PAUSADO; si temp <= umbral - HISTERESIS_C100
 *              vuelve a VIGILANDO.
 * hay_pausa vale 1 solo en el instante en que se presiona el boton. */
static estado_t estado_siguiente(estado_t actual, int32_t temp_c100, int32_t umbral_c100, int hay_pausa)
{
	switch (actual) {
	case PAUSADO:
		return hay_pausa ? VIGILANDO : PAUSADO;
	case VIGILANDO:
		if (hay_pausa)
			return PAUSADO;
		if (temp_c100 >= umbral_c100)
			return ALARMA;
		return VIGILANDO;
	case ALARMA:
		if (hay_pausa)
			return PAUSADO;
		if (temp_c100 <= umbral_c100 - (int32_t)HISTERESIS_C100)
			return VIGILANDO;
		return ALARMA;
	default:
		return actual;
	}
}

/* Escribe en dst (de n bytes) una linea del archivo, con el formato
 *   t_ms,temp_c100,umbral_c100,ESTADO\n
 * por ejemplo "1500,2534,2650,VIGILANDO\n". ESTADO es PAUSADO, VIGILANDO o ALARMA.
 * Nunca escribe mas de n bytes. Devuelve el largo que tendria la linea completa. */
static int formatear_registro(char *dst, size_t n, uint32_t t_ms, int32_t temp_c100, int32_t umbral_c100, estado_t e)
{
	const char *nombre;

	switch (e) {
	case PAUSADO:   nombre = "PAUSADO";   break;
	case VIGILANDO: nombre = "VIGILANDO"; break;
	case ALARMA:    nombre = "ALARMA";    break;
	default:               nombre = "DESCONOCIDO"; break;
	}

	if (dst == NULL)
		n = 0;
	return snprintf(dst, n, "%lu,%ld,%ld,%s\n",
	                (unsigned long)t_ms, (long)temp_c100, (long)umbral_c100, nombre);
}

#endif
