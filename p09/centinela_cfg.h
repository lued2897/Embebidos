#ifndef CENTINELA_CFG_H
#define CENTINELA_CFG_H

#include <stdint.h>

#ifndef BIT
#define BIT(n) ((uint32_t)1u << (n))
#endif

/* ---- Circuito (no se modifica) ---- */
#define LED_1 17u
#define LED_2 27u
#define LED_3 22u
#define LED_4 23u
#define LED_MASCARA (BIT(LED_1) | BIT(LED_2) | BIT(LED_3) | BIT(LED_4))
#define BOTON_PAUSA   5u   /* GPIO5: pausar y reanudar */
#define BOTON_ESTADO  6u   /* GPIO6: escribir una linea de estado en pantalla */

/* ---- Reglas del Centinela (no se modifican) ---- */
#define N_BASE            8      /* lecturas para calcular la temperatura base */
#define UMBRAL_MIN_C100   50     /* umbral minimo sobre la base: 0.50 C */
#define UMBRAL_RANGO_C100 300    /* el potenciometro recorre 3.00 C mas */
#define HISTERESIS_C100   30     /* para salir de ALARMA: 0.30 C bajo el umbral */
#define PARPADEO_MS       250    /* cambio de fase de los LEDs en ALARMA */
#define MAX_SENSOR_MS     500    /* la alarma debe salir en menos de 1 s */
#define MAX_BOTONES_MS    50     /* los botones deben responder en menos de 100 ms */
#define PERIODO_REGISTRO_MS 1000 /* una linea por segundo en el archivo */
#define ARCHIVO_REGISTRO  "centinela.csv"
#define ENCABEZADO_CSV    "t_ms,temp_c100,umbral_c100,estado\n"

typedef enum {
	PAUSADO = 0,
	VIGILANDO = 1,
	ALARMA = 2
} estado_t;

/* Devuelve 1 cuando el boton pasa de suelto (0) a presionado (1) */
static inline int flanco_subida(unsigned antes, unsigned ahora)
{
	return !antes && ahora;
}

#endif
