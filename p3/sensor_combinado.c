#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <time.h>

#define I2C_ADDR 0x68
#define REG_TEMP 0x11

#define SPI_DEVICE "/dev/spidev0.0"

#define GPIO_CHIP "/dev/gpiochip0"
#define LED_GPIO 17

#define SPI_SPEED 1000000

int main(void) {

    /* =========================
       I2C - DS3231
       ========================= */

    int i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) {
        perror("Error al abrir I2C");
        return 1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, I2C_ADDR) < 0) {
        perror("Error al configurar direccion I2C");
        close(i2c_fd);
        return 1;
    }


    /* =========================
       SPI - MCP3008
       ========================= */

    int spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        perror("Error al abrir SPI");
        close(i2c_fd);
        return 1;
    }

    unsigned char mode = SPI_MODE_0;
    unsigned char bits = 8;
    unsigned int speed = SPI_SPEED;

    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);


    /* =========================
       GPIO - LED
       ========================= */

    struct gpiod_chip *chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Error al abrir GPIO");
        close(spi_fd);
        close(i2c_fd);
        return 1;
    }

    unsigned int led_pin = LED_GPIO;

    struct gpiod_line_settings *settings =
        gpiod_line_settings_new();

    gpiod_line_settings_set_direction(
        settings,
        GPIOD_LINE_DIRECTION_OUTPUT
    );

    struct gpiod_line_config *line_cfg =
        gpiod_line_config_new();

    gpiod_line_config_add_line_settings(
        line_cfg,
        &led_pin,
        1,
        settings
    );

    struct gpiod_request_config *req_cfg =
        gpiod_request_config_new();

    gpiod_request_config_set_consumer(
        req_cfg,
        "temperatura-led"
    );

    struct gpiod_line_request *request =
        gpiod_chip_request_lines(
            chip,
            req_cfg,
            line_cfg
        );

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!request) {
        perror("Error al solicitar GPIO");
        gpiod_chip_close(chip);
        close(spi_fd);
        close(i2c_fd);
        return 1;
    }


    /* =========================
       Variables
       ========================= */

    float temperatura = 0.0f;
    float umbral = 0.0f;

    struct timespec ultima_lectura;
    clock_gettime(CLOCK_MONOTONIC, &ultima_lectura);


    /* =========================
       Programa principal
       ========================= */

    while (1) {

        /* -------------------------
           Leer potenciometro
           ------------------------- */

        unsigned char canal = 0;

        unsigned char tx[3] = {
            1,
            (8 + canal) << 4,
            0
        };

        unsigned char rx[3] = {0};

        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx,
            .rx_buf = (unsigned long)rx,
            .len = 3,
            .speed_hz = speed,
            .bits_per_word = bits
        };

        if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) >= 0) {

            int valor = ((rx[1] & 3) << 8) | rx[2];

            /* 0-1023 -> 0-50 °C */
            umbral = (valor * 50.0f) / 1023.0f;
        }


        /* -------------------------
           Leer temperatura cada 2 s
           ------------------------- */

        struct timespec ahora;
        clock_gettime(CLOCK_MONOTONIC, &ahora);

        double transcurrido =
            (ahora.tv_sec - ultima_lectura.tv_sec) +
            (ahora.tv_nsec - ultima_lectura.tv_nsec) / 1000000000.0;

        if (transcurrido >= 2.0) {

            unsigned char reg = REG_TEMP;
            unsigned char temp[2];

            if (write(i2c_fd, &reg, 1) == 1 &&
                read(i2c_fd, temp, 2) == 2) {

                int msb = (signed char)temp[0];

                temperatura =
                    msb + ((temp[1] >> 6) * 0.25f);

                printf(
                    "Temperatura: %.2f C | "
                    "ADC: %d | "
                    "Umbral: %.2f C\n",
                    temperatura,
                    (int)((umbral * 1023.0f) / 50.0f),
                    umbral
                );
            }

            ultima_lectura = ahora;
        }


        /* -------------------------
           Comparar temperatura
           ------------------------- */

        if (temperatura > umbral) {

            gpiod_line_request_set_value(
                request,
                LED_GPIO,
                GPIOD_LINE_VALUE_ACTIVE
            );

        } else {

            gpiod_line_request_set_value(
                request,
                LED_GPIO,
                GPIOD_LINE_VALUE_INACTIVE
            );
        }

        usleep(10000);
    }


    /* =========================
       Liberar recursos
       ========================= */

    gpiod_line_request_release(request);
    gpiod_chip_close(chip);

    close(spi_fd);
    close(i2c_fd);

    return 0;
}