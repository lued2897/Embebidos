/* f2_bucle.c - Promedio ponderado de las ultimas lecturas.
 * Compilar con: gcc -std=gnu11 -g -O0 -Wall -Wextra -o f2_bucle f2_bucle.c
 * Ejecutar siempre con: timeout 5 ./f2_bucle
 */
#include <stdio.h>

#define N 8

static long ponderado(const int *v, const int *pesos, int n)
{
    long acumulado = 0;
    long divisor = 0;
    long i = 1;

    while (i != n) {
        acumulado += (long)v[i % n] * pesos[i % n];
        divisor += pesos[i % n];
        i += 2;
    }
    return acumulado / divisor;
}

int main(void)
{
    int lecturas[N] = { 2410, 2415, 2432, 2498, 2561, 2530, 2477, 2440 };
    int pesos[N]    = {    1,    1,    2,    2,    3,    3,    4,    4 };

    printf("ponderado: %ld\n", ponderado(lecturas, pesos, N));
    return 0;
}
