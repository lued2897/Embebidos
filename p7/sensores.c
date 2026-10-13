#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>

static unsigned mcp3008_valor(const uint8_t rx[3])
{
    return ((unsigned)(rx[1] & 0x03u) << 8) | rx[2];
}

static int pot_leer(int fd_spi, unsigned canal, unsigned *valor)
{
    uint8_t tx[3] = {0x01, (uint8_t)((8u + canal) << 4), 0x00};
    uint8_t rx[3] = {0, 0, 0};
    struct spi_ioc_transfer tr;

    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = 3;
    tr.speed_hz = 1000000;
    tr.bits_per_word = 8;

    if (ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr) < 0)
        return -1;

    *valor = mcp3008_valor(rx);
    return 0;
}

#define DS3231_ADDR      0x68
#define DS3231_REG_TIME  0x00
#define DS3231_REG_TEMP  0x11

static uint8_t bcd_a_dec(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static int ds3231_leer_tiempo(int fd, int *h, int *m, int *s)
{
    uint8_t reg = DS3231_REG_TIME;
    uint8_t buf[3];

    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, buf, 3) != 3) return -1;

    *s = bcd_a_dec(buf[0]);
    *m = bcd_a_dec(buf[1]);
    *h = bcd_a_dec(buf[2] & 0x3F); 

    return 0;
}

static int ds3231_leer_temperatura(int fd, float *temp)
{
    uint8_t reg = DS3231_REG_TEMP;
    uint8_t buf[2];

    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, buf, 2) != 2) return -1;

    int8_t msb = (int8_t)buf[0];
    uint8_t lsb = buf[1] >> 6; 
    
    *temp = (float)msb + ((float)lsb * 0.25f);
    return 0;
}

int main(int argc, char *argv[])
{
    int direccion = (argc > 1) ? (int)strtol(argv[1], NULL, 16) : DS3231_ADDR;
    
    int fd_i2c = open("/dev/i2c-1", O_RDWR);
    int fd_spi = open("/dev/spidev0.0", O_RDWR);

    if (fd_i2c < 0 || fd_spi < 0) {
        perror("Error al abrir los buses (open)");
        return 1;
    }

    if (ioctl(fd_i2c, I2C_SLAVE, direccion) < 0) {
        perror("Error configurando esclavo I2C");
        return 1;
    }

    uint8_t modo = SPI_MODE_0;
    ioctl(fd_spi, SPI_IOC_WR_MODE, &modo);

    printf("Iniciando lectura: DS3231 en 0x%02X y MCP3008 en SPI...\n\n", direccion);

    for (int i = 0; i < 5; i++) {
        unsigned valor_pot = 0;
        int h, m, s;
        float temp;

        if (pot_leer(fd_spi, 0, &valor_pot) < 0) {
            perror("Fallo lectura SPI (MCP3008)");
        }
        
        if (ds3231_leer_tiempo(fd_i2c, &h, &m, &s) < 0) {
            perror("Fallo lectura I2C (DS3231 - Tiempo)");
        }

        if (ds3231_leer_temperatura(fd_i2c, &temp) < 0) {
            perror("Fallo lectura I2C (DS3231 - Temperatura)");
        }

        printf("SPI pot = %4u (%.2f V) | I2C Hora = %02d:%02d:%02d | I2C Temp = %.2f C\n",
               valor_pot, (double)valor_pot * 3.3 / 1023.0,
               h, m, s, temp);
               
        sleep(1);
    }

    close(fd_i2c);
    close(fd_spi);
    return 0;
}