#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_ADDR 0x68 // o 0x77, segun lo que muestre i2cdetect
#define REG_ID 0x0F

int main(void) {
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    if (ioctl(fd, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("ioctl I2C_SLAVE");
        return 1;
    }
    
    // Escribir la direccion del registro que queremos leer
    unsigned char reg = REG_ID;
    if (write(fd, &reg, 1) != 1) { perror("write"); return 1; }
    
    // Leer el valor de ese registro
    unsigned char chip_id;
    if (read(fd, &chip_id, 1) != 1) { perror("read"); return 1; }
    printf("Status leido: 0x%02X\n", chip_id);
    //printf("%s\n", chip_id == 0x58 ? "Es un BMP280 real" : "Algo no cuadra");
    close(fd);
    return 0;
}
