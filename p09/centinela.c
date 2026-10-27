/* Centinela: alarma termica con el circuito de las practicas 2 y 3.
 *
 *   BMP280       temperatura
 *   Potenciometro umbral de alarma (base + 0.50 C ... base + 3.50 C)
 *   LEDs         barra de margen, o parpadeo en ALARMA
 *   Boton GPIO5  pausa y reanuda
 *   Boton GPIO6  escribe una linea de estado en pantalla
 *   centinela.csv  una linea por segundo y una en cada cambio de estado
 *
 * Compilar:  make centinela      (en la Pi)
 *            make sim            (simulado, en cualquier computadora)
 * Ejecutar:  ./centinela [direccion_BMP280_en_hexadecimal]    Ctrl+C para salir
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "centinela_cfg.h"
#include "centinela_logica.h"
#include "hw.h"

/* Periodos del programa. Se pueden cambiar entre 1 y el maximo de centinela_cfg.h. */
#define PERIODO_BOTONES_MS 50
#define PERIODO_SENSOR_MS  250

_Static_assert(PERIODO_BOTONES_MS >= 1 && PERIODO_BOTONES_MS <= MAX_BOTONES_MS,
	       "PERIODO_BOTONES_MS fuera de la regla");
_Static_assert(PERIODO_SENSOR_MS >= 1 && PERIODO_SENSOR_MS <= MAX_SENSOR_MS,
	       "PERIODO_SENSOR_MS fuera de la regla");

static volatile sig_atomic_t salir = 0;

static void al_salir(int senal)
{
	(void)senal;
	salir = 1;
}

static uint32_t ahora_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static const char *texto_estado(estado_t e)
{
	switch (e) {
	case PAUSADO:   return "PAUSADO";
	case VIGILANDO: return "VIGILANDO";
	case ALARMA:    return "ALARMA";
	}
	return "?";
}

static void imprimir_temp(int32_t c)
{
	uint32_t a = (uint32_t)(c < 0 ? -c : c);

	printf("%s%u.%02u", c < 0 ? "-" : "", a / 100u, a % 100u);
}

static FILE *abrir_registro(void)
{
	FILE *f = fopen(ARCHIVO_REGISTRO, "a");

	if (f == NULL) {
		perror("fopen " ARCHIVO_REGISTRO);
		return NULL;
	}
	if (ftell(f) == 0)
		fputs(ENCABEZADO_CSV, f);
	return f;
}

/* Escribe una linea en el archivo. Con sincronizar = 1 la fuerza a llegar a la microSD. */
static void registrar(FILE *f, uint32_t t, int32_t temp, int32_t umbral, estado_t e, int sincronizar)
{
	char linea[96];

	formatear_registro(linea, sizeof(linea), t, temp, umbral, e);
	fputs(linea, f);
	fflush(f);
	if (sincronizar)
		fsync(fileno(f));
}

int main(int argc, char *argv[])
{
	int direccion = (argc > 1) ? (int)strtol(argv[1], NULL, 16) : 0x76;
	hw_t hw;
	FILE *reg;
	int32_t lecturas[N_BASE], base, temp = 0, umbral;
	unsigned pot = 0, b_antes;
	estado_t estado = VIGILANDO;
	uint32_t t0, t_sensor = 0, t_registro = 0, mascara_actual = 0xFFFFFFFFu;

	if (hw_abrir(&hw, direccion) < 0)
		return 1;
	reg = abrir_registro();
	if (reg == NULL) {
		hw_cerrar(&hw);
		return 1;
	}
	signal(SIGINT, al_salir);
	signal(SIGTERM, al_salir);

	for (int i = 0; i < N_BASE; i++) {
		if (hw_temp(&hw, &lecturas[i]) < 0)
			lecturas[i] = 0;
		usleep(100000);
	}
	base = temp_base(lecturas, N_BASE);
	hw_pot(&hw, &pot);
	umbral = umbral_desde_pot(base, pot);
	temp = base;
	printf("Base: ");
	imprimir_temp(base);
	printf(" C. Boton GPIO%u: pausa. Boton GPIO%u: estado. Ctrl+C: salir.\n", BOTON_PAUSA, BOTON_ESTADO);

	t0 = ahora_ms();
	b_antes = hw_botones(&hw);
	registrar(reg, 0, temp, umbral, estado, 1);
	while (!salir) {
		uint32_t t = ahora_ms() - t0;
		unsigned b = hw_botones(&hw);
		int hay_pausa = flanco_subida(b_antes & 1u, b & 1u);
		int pide_estado = flanco_subida((b_antes >> 1) & 1u, (b >> 1) & 1u);
		estado_t nuevo;
		uint32_t mascara;

		b_antes = b;
		if (t - t_sensor >= PERIODO_SENSOR_MS) {
			t_sensor = t;
			if (hw_temp(&hw, &temp) < 0)
				fprintf(stderr, "Aviso: no se pudo leer el BMP280\n");
			if (hw_pot(&hw, &pot) < 0)
				fprintf(stderr, "Aviso: no se pudo leer el potenciometro\n");
			umbral = umbral_desde_pot(base, pot);
		}

		nuevo = estado_siguiente(estado, temp, umbral, hay_pausa);
		if (nuevo != estado) {
			estado = nuevo;
			registrar(reg, t, temp, umbral, estado, 1);
			t_registro = t;
		} else if (t - t_registro >= PERIODO_REGISTRO_MS) {
			registrar(reg, t, temp, umbral, estado, 0);
			t_registro = t;
		}

		if (pide_estado) {
			printf("%s  T = ", texto_estado(estado));
			imprimir_temp(temp);
			printf(" C  umbral = ");
			imprimir_temp(umbral);
			printf(" C  pot = %u\n", pot);
		}

		switch (estado) {
		case PAUSADO:
			mascara = 0;
			break;
		case VIGILANDO:
			mascara = mascara_leds(nivel_barra(umbral - temp));
			break;
		default:
			mascara = mascara_alarma((unsigned)(t / PARPADEO_MS));
			break;
		}
		if (mascara != mascara_actual) {
			hw_leds(&hw, mascara);
			mascara_actual = mascara;
		}
		usleep(PERIODO_BOTONES_MS * 1000u);
	}

	registrar(reg, ahora_ms() - t0, temp, umbral, estado, 1);
	fclose(reg);
	hw_cerrar(&hw);
	printf("Centinela detenido\n");
	return 0;
}
