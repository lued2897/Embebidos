#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_ADDR 0x68
#define REG_TEMP 0x11

int main(void) {
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    if (ioctl(fd, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("ioctl I2C_SLAVE");
        return 1;
    }
    
    // Escribir la direccion del registro que queremos leer
    unsigned char reg = REG_TEMP;
    if (write(fd, &reg, 1) != 1) { perror("write"); return 1; }
    
    // Leer MSB y LSB de temperatura
    unsigned char temp[2];
    if (read(fd, temp, 2) != 2) { perror("read"); return 1; }

    // Convertir la temperatura
    int msb = (signed char)temp[0];
    float temperatura = msb + ((temp[1] >> 6) * 0.25);

    printf("Temperatura: %.2f °C\n", temperatura);

    close(fd);
    return 0;
}