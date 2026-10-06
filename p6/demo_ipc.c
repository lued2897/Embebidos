#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(void) {
    int valor = 10;      // variable que "parece" compartida
    int fd[2];
    pipe(fd);             // fd[0]=lectura, fd[1]=escritura

    pid_t pid = fork();

    if (pid == 0) {
        // ---- Proceso HIJO ----
        close(fd[0]);

        valor = 99;         // modifica SU PROPIA copia
        printf("[HIJO]  valor local = %d\n", valor);

        char msg[] = "Hola desde el hijo";
        write(fd[1], msg, strlen(msg) + 1);
        close(fd[1]);
        exit(0);
    }
    else {
            // ---- Proceso PADRE ----
            close(fd[1]);

            sleep(1);   // da tiempo a que el hijo imprima primero

            printf("[PADRE] valor local = %d ", valor);
            printf("(nunca cambio, aunque el hijo lo modifico)\n");

            char buffer[100];
            read(fd[0], buffer, sizeof(buffer));
            close(fd[0]);
            printf("[PADRE] recibido por el pipe: %s\n", buffer);

            wait(NULL);
        }
        return 0;
}