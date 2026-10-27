/* estadistica.c - Resumen de lecturas de temperatura (centesimas de grado).
 * Programa de la Parte B. Compilar con: gcc -std=gnu11 -g -O0 -Wall -Wextra -o estadistica estadistica.c
 */
#include <stdio.h>

#define N_LECTURAS 8

static int suma(const int *v, int n)
{
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += v[i];
    }
    return total;
}

static int promedio(const int *v, int n)
{
    int total = suma(v, n);
    return total / n;
}

static int maximo(const int *v, int n)
{
    int mayor = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] > mayor) {
            mayor = v[i];
        }
    }
    return mayor;
}

static int rango(const int *v, int n)
{
    int menor = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] < menor) {
            menor = v[i];
        }
    }
    return maximo(v, n) - menor;
}

static void resumen(const int *v, int n)
{
    int prom = promedio(v, n);
    int mayor = maximo(v, n);
    int dif = rango(v, n);

    printf("lecturas: %d\n", n);
    printf("promedio: %d.%02d C\n", prom / 100, prom % 100);
    printf("maximo:   %d.%02d C\n", mayor / 100, mayor % 100);
    printf("rango:    %d.%02d C\n", dif / 100, dif % 100);
}

int main(void)
{
    int lecturas[N_LECTURAS] = { 2410, 2415, 2432, 2498, 2561, 2530, 2477, 2440 };

    resumen(lecturas, N_LECTURAS);
    return 0;
}
