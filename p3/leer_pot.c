#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
int main(void) {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    unsigned char mode = SPI_MODE_0;
    unsigned char bits = 8;
    unsigned int speed = 1000000;
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    while (1) {
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
        printf("Valor: %4d Voltaje: %.2fV\n", valor, voltaje);
        
        usleep(300000);
    }
    
    close(fd);
    return 0;
}