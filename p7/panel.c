#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include "gpio_regs.h"

#define BOTON_5 5
#define BOTON_6 6
static const unsigned leds[4] = {17, 27, 22, 23};
#define LED_MASCARA (BIT(17) | BIT(27) | BIT(22) | BIT(23))

static const unsigned pines_criticos[] = {2, 3, 8, 9, 10, 11};
#define NUM_CRITICOS (sizeof(pines_criticos) / sizeof(pines_criticos[0]))

volatile sig_atomic_t salir = 0;

static void manejador_sigint(int sig)
{
    (void)sig;
    salir = 1;
}

static uint32_t obtener_funcion(gpio_regs_t *gpio, unsigned pin)
{
    unsigned reg = pin / 10;
    unsigned desp = (pin % 10) * 3;
    return (gpio->GPFSEL[reg] >> desp) & 0x7u;
}

static void configurar(gpio_regs_t *gpio, unsigned pin, uint32_t funcion)
{
    unsigned reg = pin / 10;
    unsigned desp = (pin % 10) * 3;
    uint32_t valor = gpio->GPFSEL[reg];
    valor &= ~((uint32_t)0x7u << desp);
    valor |= ((funcion & 0x7u) << desp);
    gpio->GPFSEL[reg] = valor;
}

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
#define DS3231_REG_TEMP  0x11

static int ds3231_leer_temperatura_cent(int fd, int32_t *temp_cent)
{
    uint8_t reg = DS3231_REG_TEMP;
    uint8_t buf[2];

    if (write(fd, &reg, 1) != 1) return -1;
    if (read(fd, buf, 2) != 2) return -1;

    int8_t msb = (int8_t)buf[0];
    uint8_t lsb = (uint8_t)(buf[1] >> 6);
    
    *temp_cent = (int32_t)msb * 100 + (int32_t)lsb * 25;
    return 0;
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejador_sigint;
    sigaction(SIGINT, &sa, NULL);

    gpio_regs_t *gpio = gpio_abrir();
    if (gpio == NULL) {
        fprintf(stderr, "Error abriendo GPIO.\n");
        return 1;
    }

    uint32_t func_criticos_orig[NUM_CRITICOS];
    for (unsigned i = 0; i < NUM_CRITICOS; i++) {
        func_criticos_orig[i] = obtener_funcion(gpio, pines_criticos[i]);
    }

    for (int i = 0; i < 4; i++) {
        configurar(gpio, leds[i], 1);
    }
    configurar(gpio, BOTON_5, 0);
    configurar(gpio, BOTON_6, 0);

    int fd_spi = open("/dev/spidev0.0", O_RDWR);
    int fd_i2c = open("/dev/i2c-1", O_RDWR);
    if (fd_spi < 0 || fd_i2c < 0) {
        perror("Error al abrir buses");
        return 1;
    }
    
    uint8_t modo = SPI_MODE_0;
    ioctl(fd_spi, SPI_IOC_WR_MODE, &modo);
    ioctl(fd_i2c, I2C_SLAVE, DS3231_ADDR);

    int congelado = 0;
    int estado_b5_ant = 0;
    int estado_b6_ant = 0;
    unsigned pot = 0;

    printf("--- Panel iniciado. Ctrl+C para salir ---\n");

    while (!salir) {
        pot_leer(fd_spi, 0, &pot);

        int b5_actual = (gpio->GPLEV[0] & BIT(BOTON_5)) ? 1 : 0;
        if (b5_actual == 1 && estado_b5_ant == 0) {
            congelado = !congelado;
        }
        estado_b5_ant = b5_actual;

        int b6_actual = (gpio->GPLEV[0] & BIT(BOTON_6)) ? 1 : 0;
        if (b6_actual == 1 && estado_b6_ant == 0) {
            int32_t t_cent;
            if (ds3231_leer_temperatura_cent(fd_i2c, &t_cent) == 0) {
                int32_t abs_cent = (t_cent < 0) ? -t_cent : t_cent;
                printf("Temperatura DS3231: %d.%02d C | Pot: %4u\n", 
                       t_cent / 100, (int)(abs_cent % 100), pot);
            } else {
                printf("Error leyendo DS3231\n");
            }
        }
        estado_b6_ant = b6_actual;

        if (!congelado) {
            unsigned tramo = pot / 205;
            if (tramo > 4) tramo = 4;

            uint32_t mask_set = 0;
            if (tramo > 0) mask_set |= BIT(17);
            if (tramo > 1) mask_set |= BIT(27);
            if (tramo > 2) mask_set |= BIT(22);
            if (tramo > 3) mask_set |= BIT(23);

            uint32_t mask_clr = LED_MASCARA & ~mask_set;

            if (mask_set > 0) gpio->GPSET[0] = mask_set;
            if (mask_clr > 0) gpio->GPCLR[0] = mask_clr;
        }

        usleep(30000);
    }

    printf("\nSaliendo y limpiando...\n");

    gpio->GPCLR[0] = LED_MASCARA;
    for (int i = 0; i < 4; i++) {
        configurar(gpio, leds[i], 0);
    }

    int alertas = 0;
    for (unsigned i = 0; i < NUM_CRITICOS; i++) {
        unsigned pin = pines_criticos[i];
        uint32_t func_actual = obtener_funcion(gpio, pin);
        int nivel_alto = (gpio->GPLEV[0] & BIT(pin)) ? 1 : 0;

        if (func_actual != func_criticos_orig[i]) {
            printf("El pin GPIO%u cambió de función (era %u, ahora es %u)\n", 
                   pin, func_criticos_orig[i], func_actual);
            alertas++;
        }
        if (!nivel_alto) {
            printf("El pin GPIO%u no está en ALTO\n", pin);
            alertas++;
        }
    }

    if (alertas == 0) {
        printf("Pines I2C/SPI en ALTO.\n");
    }

    close(fd_spi);
    close(fd_i2c);
    return 0;
}