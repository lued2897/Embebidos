#include <stdio.h>
#include <pthread.h>
 
#define ITER 1000000
long contador = 0;
 
void *sumar(void *arg) {
    for (int i = 0; i < ITER; i++) contador++;
    return NULL;
}
 
int main(void) {
    pthread_t h1, h2;
    pthread_create(&h1, NULL, sumar, NULL);
    pthread_create(&h2, NULL, sumar, NULL);
    pthread_join(h1, NULL);
    pthread_join(h2, NULL);
    printf("Esperado: %d  Obtenido: %ld\n", 2 * ITER, contador);
    return 0;
}
