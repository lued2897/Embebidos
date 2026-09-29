#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h> // Para usleep()

#define TAM_BUFFER 8
#define MAX_NUMEROS 100

// Variables compartidas
float buffer[TAM_BUFFER];
int pos_entrada = 0; // Índice para el productor
int pos_salida = 0;  // Índice para el consumidor

int numeros_producidos = 0;
int numeros_consumidos = 0;

// Sincronización
pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_huecos;    // Controla el espacio disponible en el buffer
sem_t sem_elementos; // Controla los datos listos para ser consumidos

// Hilo Productor
void *productor(void *arg) {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) { perror("open"); return 0; }

    unsigned char mode = SPI_MODE_0;
    unsigned char bits = 8;
    unsigned int speed = 1000000;
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    for (int i = 1; i <= MAX_NUMEROS; i++) {
        unsigned char canal = 0;
        unsigned char tx[3] = {1, (8 + canal) << 4, 0};
        unsigned char rx[3] = {0};

        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx,
            .rx_buf = (unsigned long)rx,

            .len = 3,
            .speed_hz = speed,
            .bits_per_word = bits,
        };

        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

        int valor = ((rx[1] & 3) << 8) | rx[2]; // 0-1023
        float voltaje = (valor * 3.3f) / 1023.0f;
    
        sem_wait(&sem_huecos);        // Espera si el buffer está lleno
        pthread_mutex_lock(&candado); // Inicia sección crítica

        // Insertar en el buffer circular
        buffer[pos_entrada] = voltaje;
        pos_entrada = (pos_entrada + 1) % TAM_BUFFER;
        numeros_producidos++;
        
        printf("Productor Creó: %.2f\n", voltaje);

        pthread_mutex_unlock(&candado); // Termina sección crítica
        sem_post(&sem_elementos);

        
        usleep(300000);
    }
    
    close(fd);
    return 0;



    for (int i = 1; i <= MAX_NUMEROS; i++) {
        usleep(100000); // 1. Genera un número (espera 100 ms = 100,000 microsegundos)

        sem_wait(&sem_huecos);        // Espera si el buffer está lleno
        pthread_mutex_lock(&candado); // Inicia sección crítica

        // Insertar en el buffer circular
        buffer[pos_entrada] = i;
        pos_entrada = (pos_entrada + 1) % TAM_BUFFER;
        numeros_producidos++;
        
        printf("Productor Creó: %d\n", i);

        pthread_mutex_unlock(&candado); // Termina sección crítica
        sem_post(&sem_elementos);       // Avisa que hay un nuevo elemento listo
    }
    return NULL;
}


// Hilo Consumidor
void *consumidor(void *arg) {
    for (int i = 0; i < MAX_NUMEROS; i++) {
        sem_wait(&sem_elementos);     // Espera si el buffer está vacío
        pthread_mutex_lock(&candado); // Inicia sección crítica

        // Extraer del buffer circular
        float dato = buffer[pos_salida];
        pos_salida = (pos_salida + 1) % TAM_BUFFER;
        numeros_consumidos++;

        pthread_mutex_unlock(&candado); // Termina sección crítica
        sem_post(&sem_huecos);          // Avisa que se liberó un hueco

        // 2. Lee, imprime y procesa el número (espera 300 ms)
        printf("Consumidor procesó: %.2f\n", dato);
        usleep(300000); 
    }
    return NULL;
}

int main(void) {
    pthread_t h_prod, h_cons;

    // Inicialización de semáforos (0 = se comparten entre hilos del mismo proceso)
    sem_init(&sem_huecos, 0, TAM_BUFFER); // Inicia con 8 huecos disponibles
    sem_init(&sem_elementos, 0, 0);       // Inicia con 0 elementos listos

    // Crear hilos
    pthread_create(&h_prod, NULL, productor, NULL);
    pthread_create(&h_cons, NULL, consumidor, NULL);

    // Esperar a que los hilos terminen
    pthread_join(h_prod, NULL);
    pthread_join(h_cons, NULL);

    // Resultados finales
    printf("\n=== Resumen de ejecución ===\n");
    printf("Números producidos: %d\n", numeros_producidos);
    printf("Números consumidos: %d\n", numeros_consumidos);

    // Limpieza de recursos
    sem_destroy(&sem_huecos);
    sem_destroy(&sem_elementos);
    pthread_mutex_destroy(&candado);

    return 0;
}