#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
 
#define N 2000
 
void *vacio(void *arg) { return NULL; }
 
double ahora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
 
int main(void) {
    double t0 = ahora();
    for (int i = 0; i < N; i++) {
        pid_t pid = fork();
        if (pid == 0) _exit(0);
        waitpid(pid, NULL, 0);
    }
    double t1 = ahora();
    for (int i = 0; i < N; i++) {
        pthread_t h;
        pthread_create(&h, NULL, vacio, NULL);
        pthread_join(h, NULL);
    }
    double t2 = ahora();
    printf("%d procesos: %.3f s\n", N, t1 - t0);
    printf("%d hilos:    %.3f s\n", N, t2 - t1);
    return 0;
}
