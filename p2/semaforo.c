#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define CHIP_NAME "/dev/gpiochip0"

#define GPIO_RED       17
#define GPIO_YELLOW    27
#define GPIO_GREEN     22
#define GPIO_BUTTON    5
#define GPIO_CROSS     23

static volatile int running = 1;

void signal_handler(int signal)
{
    running = 0;
}

int leer_boton(struct gpiod_line_request *request)
{
    enum gpiod_line_value value;

    value = gpiod_line_request_get_value(request, GPIO_BUTTON);

    return value == GPIOD_LINE_VALUE_ACTIVE;
}

void esperar(struct gpiod_line_request *request,
             int segundos,
             int *peticion)
{
    int ciclos = segundos * 10;

    for (int i = 0; i < ciclos && running; i++) {

        if (leer_boton(request))
            *peticion = 1;

        usleep(100000);
    }
}

void rojo_peatonal(struct gpiod_line_request *request)
{
    gpiod_line_request_set_value(
        request,
        GPIO_RED,
        GPIOD_LINE_VALUE_ACTIVE
    );

    for (int i = 0; i < 16 && running; i++) {

        gpiod_line_request_set_value(
            request,
            GPIO_CROSS,
            GPIOD_LINE_VALUE_ACTIVE
        );

        usleep(250000);

        gpiod_line_request_set_value(
            request,
            GPIO_CROSS,
            GPIOD_LINE_VALUE_INACTIVE
        );

        usleep(250000);
    }

    gpiod_line_request_set_value(
        request,
        GPIO_CROSS,
        GPIOD_LINE_VALUE_INACTIVE
    );
}

void apagar_leds(struct gpiod_line_request *request)
{
    gpiod_line_request_set_value(
        request,
        GPIO_RED,
        GPIOD_LINE_VALUE_INACTIVE
    );

    gpiod_line_request_set_value(
        request,
        GPIO_YELLOW,
        GPIOD_LINE_VALUE_INACTIVE
    );

    gpiod_line_request_set_value(
        request,
        GPIO_GREEN,
        GPIOD_LINE_VALUE_INACTIVE
    );

    gpiod_line_request_set_value(
        request,
        GPIO_CROSS,
        GPIOD_LINE_VALUE_INACTIVE
    );
}

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *output_settings;
    struct gpiod_line_settings *input_settings;
    struct gpiod_line_config *line_config;
    struct gpiod_request_config *request_config;
    struct gpiod_line_request *request;

    int peticion = 0;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    chip = gpiod_chip_open(CHIP_NAME);

    if (!chip) {
        perror("No se pudo abrir gpiochip0");
        return 1;
    }

    output_settings = gpiod_line_settings_new();

    if (!output_settings) {
        perror("No se pudieron crear los ajustes de salida");
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_settings_set_direction(
        output_settings,
        GPIOD_LINE_DIRECTION_OUTPUT
    );

    gpiod_line_settings_set_output_value(
        output_settings,
        GPIOD_LINE_VALUE_INACTIVE
    );

    input_settings = gpiod_line_settings_new();

    if (!input_settings) {
        perror("No se pudieron crear los ajustes de entrada");
        gpiod_line_settings_free(output_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_settings_set_direction(
        input_settings,
        GPIOD_LINE_DIRECTION_INPUT
    );

    line_config = gpiod_line_config_new();

    if (!line_config) {
        perror("No se pudo crear la configuracion GPIO");
        gpiod_line_settings_free(output_settings);
        gpiod_line_settings_free(input_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    unsigned int salidas[] = {
        GPIO_RED,
        GPIO_YELLOW,
        GPIO_GREEN,
        GPIO_CROSS
    };

    for (int i = 0; i < 4; i++) {
        if (gpiod_line_config_add_line_settings(
                line_config,
                &salidas[i],
                1,
                output_settings) < 0) {

            perror("Error configurando salida");

            gpiod_line_config_free(line_config);
            gpiod_line_settings_free(output_settings);
            gpiod_line_settings_free(input_settings);
            gpiod_chip_close(chip);

            return 1;
        }
    }

    if (gpiod_line_config_add_line_settings(
            line_config,
            (unsigned int[]){GPIO_BUTTON},
            1,
            input_settings) < 0) {

        perror("Error configurando boton");

        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(output_settings);
        gpiod_line_settings_free(input_settings);
        gpiod_chip_close(chip);

        return 1;
    }

    request_config = gpiod_request_config_new();

    if (!request_config) {
        perror("No se pudo crear request_config");

        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(output_settings);
        gpiod_line_settings_free(input_settings);
        gpiod_chip_close(chip);

        return 1;
    }

    gpiod_request_config_set_consumer(
        request_config,
        "semaforo"
    );

    request = gpiod_chip_request_lines(
        chip,
        request_config,
        line_config
    );

    if (!request) {
        perror("No se pudieron solicitar los GPIO");

        gpiod_request_config_free(request_config);
        gpiod_line_config_free(line_config);
        gpiod_line_settings_free(output_settings);
        gpiod_line_settings_free(input_settings);
        gpiod_chip_close(chip);

        return 1;
    }

    printf("Semaforo iniciado\n");

    while (running) {

        gpiod_line_request_set_value(
            request,
            GPIO_GREEN,
            GPIOD_LINE_VALUE_ACTIVE
        );

        esperar(request, 5, &peticion);

        gpiod_line_request_set_value(
            request,
            GPIO_GREEN,
            GPIOD_LINE_VALUE_INACTIVE
        );


        gpiod_line_request_set_value(
            request,
            GPIO_YELLOW,
            GPIOD_LINE_VALUE_ACTIVE
        );

        esperar(request, 2, &peticion);

        gpiod_line_request_set_value(
            request,
            GPIO_YELLOW,
            GPIOD_LINE_VALUE_INACTIVE
        );


        gpiod_line_request_set_value(
            request,
            GPIO_RED,
            GPIOD_LINE_VALUE_ACTIVE
        );

        if (peticion) {

            printf("Cruce peatonal solicitado\n");

            rojo_peatonal(request);

            peticion = 0;

        } else {

            esperar(request, 5, &peticion);
        }


        gpiod_line_request_set_value(
            request,
            GPIO_RED,
            GPIOD_LINE_VALUE_INACTIVE
        );
    }

    apagar_leds(request);

    gpiod_line_request_release(request);

    gpiod_request_config_free(request_config);
    gpiod_line_config_free(line_config);
    gpiod_line_settings_free(output_settings);
    gpiod_line_settings_free(input_settings);

    gpiod_chip_close(chip);

    printf("Semaforo detenido\n");

    return 0;
}