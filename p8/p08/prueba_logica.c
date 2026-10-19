/* Pruebas de la logica del Centinela. No usa hardware: corre en cualquier computadora.
 *   make prueba
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "centinela_cfg.h"
#include "centinela_logica.h"

static int total = 0, pasadas = 0;

#define VERIFICA(desc, cond) do { \
	total++; \
	if (cond) { pasadas++; printf("  [ok]    %s\n", desc); } \
	else { printf("  [FALLA] %s\n", desc); } \
} while (0)

#define SECCION(t) printf("\n%s\n", t)

static int registro_igual(uint32_t t, int32_t temp, int32_t umbral, estado_t e, const char *esperado)
{
	char buf[64];
	int n;

	memset(buf, 'X', sizeof(buf));
	n = formatear_registro(buf, sizeof(buf), t, temp, umbral, e);
	return n == (int)strlen(esperado) && strcmp(buf, esperado) == 0;
}

int main(void)
{
	SECCION("temp_base");
	{
		int32_t a[5] = {2500, 2510, 2490, 2500, 2500};
		int32_t b[2] = {2501, 2502};
		int32_t c[4] = {-500, -400, -300, -200};
		int32_t d[3] = {2147483000, 2147483000, 2147483000};

		VERIFICA("promedio de cinco lecturas = 2500", temp_base(a, 5) == 2500);
		VERIFICA("promedio entero de 2501 y 2502 = 2501", temp_base(b, 2) == 2501);
		VERIFICA("promedio de valores negativos = -350", temp_base(c, 4) == -350);
		VERIFICA("la suma no desborda con valores grandes", temp_base(d, 3) == 2147483000);
	}

	SECCION("umbral_desde_pot");
	VERIFICA("pot = 0 da base + 50", umbral_desde_pot(2500, 0) == 2550);
	VERIFICA("pot = 1023 da base + 350", umbral_desde_pot(2500, 1023) == 2850);
	VERIFICA("pot = 512 da base + 200", umbral_desde_pot(2500, 512) == 2700);
	VERIFICA("funciona con base negativa", umbral_desde_pot(-500, 0) == -450);
	VERIFICA("pot = 341 da base + 150", umbral_desde_pot(2000, 341) == 2150);

	SECCION("nivel_barra");
	VERIFICA("margen 401 -> 0 LEDs", nivel_barra(401) == 0);
	VERIFICA("margen 400 -> 1 LED", nivel_barra(400) == 1);
	VERIFICA("margen 201 -> 1 LED", nivel_barra(201) == 1);
	VERIFICA("margen 200 -> 2 LEDs", nivel_barra(200) == 2);
	VERIFICA("margen 101 -> 2 LEDs", nivel_barra(101) == 2);
	VERIFICA("margen 100 -> 3 LEDs", nivel_barra(100) == 3);
	VERIFICA("margen 1 -> 3 LEDs", nivel_barra(1) == 3);
	VERIFICA("margen 0 -> 4 LEDs", nivel_barra(0) == 4);
	VERIFICA("margen -250 -> 4 LEDs", nivel_barra(-250) == 4);

	SECCION("mascara_leds");
	VERIFICA("0 LEDs -> mascara 0", mascara_leds(0) == 0);
	VERIFICA("1 LED -> solo GPIO17", mascara_leds(1) == BIT(17));
	VERIFICA("2 LEDs -> GPIO17 y 27", mascara_leds(2) == (BIT(17) | BIT(27)));
	VERIFICA("3 LEDs -> GPIO17, 27 y 22", mascara_leds(3) == (BIT(17) | BIT(27) | BIT(22)));
	VERIFICA("4 LEDs -> los cuatro", mascara_leds(4) == LED_MASCARA);
	VERIFICA("mas de 4 se limita a los cuatro", mascara_leds(9) == LED_MASCARA);

	SECCION("mascara_alarma");
	VERIFICA("fase 0 -> GPIO17 y 22", mascara_alarma(0) == (BIT(17) | BIT(22)));
	VERIFICA("fase 1 -> GPIO27 y 23", mascara_alarma(1) == (BIT(27) | BIT(23)));
	VERIFICA("fase 2 repite la fase 0", mascara_alarma(2) == (BIT(17) | BIT(22)));
	VERIFICA("fase 7 repite la fase 1", mascara_alarma(7) == (BIT(27) | BIT(23)));

	SECCION("estado_siguiente");
	VERIFICA("VIGILANDO con temperatura bajo el umbral se queda", estado_siguiente(VIGILANDO, 2699, 2700, 0) == VIGILANDO);
	VERIFICA("VIGILANDO con temperatura igual al umbral pasa a ALARMA", estado_siguiente(VIGILANDO, 2700, 2700, 0) == ALARMA);
	VERIFICA("VIGILANDO con el boton de pausa pasa a PAUSADO", estado_siguiente(VIGILANDO, 2500, 2700, 1) == PAUSADO);
	VERIFICA("ALARMA con temperatura sobre el umbral se queda", estado_siguiente(ALARMA, 2800, 2700, 0) == ALARMA);
	VERIFICA("ALARMA a 29 centesimas bajo el umbral se queda", estado_siguiente(ALARMA, 2671, 2700, 0) == ALARMA);
	VERIFICA("ALARMA a 30 centesimas bajo el umbral vuelve a VIGILANDO", estado_siguiente(ALARMA, 2670, 2700, 0) == VIGILANDO);
	VERIFICA("ALARMA con el boton de pausa pasa a PAUSADO", estado_siguiente(ALARMA, 2800, 2700, 1) == PAUSADO);
	VERIFICA("PAUSADO ignora la temperatura alta", estado_siguiente(PAUSADO, 3000, 2700, 0) == PAUSADO);
	VERIFICA("PAUSADO con el boton de pausa vuelve a VIGILANDO", estado_siguiente(PAUSADO, 3000, 2700, 1) == VIGILANDO);
	VERIFICA("PAUSADO sin boton se queda aunque este frio", estado_siguiente(PAUSADO, 2000, 2700, 0) == PAUSADO);

	SECCION("formatear_registro");
	VERIFICA("linea VIGILANDO", registro_igual(1500, 2534, 2650, VIGILANDO, "1500,2534,2650,VIGILANDO\n"));
	VERIFICA("linea ALARMA", registro_igual(0, 2800, 2700, ALARMA, "0,2800,2700,ALARMA\n"));
	VERIFICA("linea PAUSADO con temperatura negativa", registro_igual(61000, -120, 300, PAUSADO, "61000,-120,300,PAUSADO\n"));
	{
		char guarda[16];
		int (*volatile formatear)(char *, size_t, uint32_t, int32_t, int32_t, estado_t) = formatear_registro;
		int n;

		memset(guarda, 'G', sizeof(guarda));
		n = formatear(guarda, 10, 123456, 2534, 2650, ALARMA);
		VERIFICA("con buffer corto no escribe mas de n bytes", memchr(guarda, '\0', 10) != NULL && guarda[10] == 'G' && guarda[15] == 'G');
		VERIFICA("con buffer corto devuelve el largo que necesitaria", n == (int)strlen("123456,2534,2650,ALARMA\n"));
	}

	printf("\nPRUEBAS: %d de %d\n", pasadas, total);
	return pasadas == total ? 0 : 1;
}
