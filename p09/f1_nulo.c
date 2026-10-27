/* f1_nulo.c - Pico de temperatura entre las lecturas validas.
 * Compilar con: gcc -std=gnu11 -g -O0 -Wall -Wextra -o f1_nulo f1_nulo.c
 */
#include <stdio.h>

#define N 6
#define MIN_VALIDA (-4000)
#define MAX_VALIDA 8500

/* Copia a salida las lecturas dentro del rango del sensor y devuelve cuantas copio. */
static int filtrar(const int *entrada, int n, int *salida)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (entrada[i] >= MIN_VALIDA && entrada[i] <= MAX_VALIDA) {
            salida[k] = entrada[i];
            k++;
        }
    }
    return k;
}

/* Devuelve la direccion de la lectura mas alta. */
static const int *buscar_pico(const int *v, int n)
{
    const int *pico = NULL;
    for (int i = 0; i < n; i++) {
        if (pico == NULL || v[i] > *pico) {
            pico = &v[i];
        }
    }
    return pico;
}

int main(void)
{
    /* Lecturas de una corrida con el sensor desconectado. */
    int crudas[N] = { -9999, -9999, -9999, -9999, -9999, -9999 };
    int validas[N];
    int cuantas = filtrar(crudas, N, validas);
    const int *pico = buscar_pico(validas, cuantas);

    printf("validas: %d\n", cuantas);
    printf("pico: %d\n", *pico);
    return 0;
}
