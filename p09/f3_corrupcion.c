/* f3_corrupcion.c - Configuracion de la alarma.
 * Compilar con: gcc -std=gnu11 -g -O0 -Wall -Wextra -o f3_corrupcion f3_corrupcion.c
 */
#include <stdio.h>
#include <string.h>

struct config {
    char etiqueta[8];
    int umbral;
};

static struct config cfg;

static void iniciar(void)
{
    strcpy(cfg.etiqueta, "cocina");
    cfg.umbral = 2700;
}

static void renombrar(const char *nombre)
{
    strcpy(cfg.etiqueta, nombre);
}

static int excede(int temperatura)
{
    return temperatura >= cfg.umbral;
}

int main(void)
{
    iniciar();
    printf("%s: umbral %d, 26.50 C -> %d\n", cfg.etiqueta, cfg.umbral, excede(2650));

    renombrar("cuarto de maquinas");
    printf("%s: umbral %d, 26.50 C -> %d\n", cfg.etiqueta, cfg.umbral, excede(2650));
    return 0;
}
