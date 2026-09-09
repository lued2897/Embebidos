#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#define CHIP_PATH "/dev/gpiochip4" // En RPi OS v2 suele ser gpiochip4 o /dev/gpiochip0
#define N_LEDS 4

int main(void) {
    unsigned int pines[N_LEDS] = {17, 27, 22, 23};
    
    // 1. Abrir el chip GPIO
    struct gpiod_chip *chip = gpiod_chip_open(CHIP_PATH);
    if (!chip) {
        perror("Error al abrir el chip GPIO");
        return 1;
    }

    // 2. Configurar las líneas como salidas
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    for (int i = 0; i < N_LEDS; i++) {
        gpiod_line_config_add_line_settings(line_cfg, &pines[i], 1, settings);
    }

    // 3. Crear las opciones de petición y solicitar las líneas
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "leds-secuencia");

    struct gpiod_line_request *request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    
    // Liberar estructuras auxiliares de configuración
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!request) {
        perror("Error al solicitar las lineas GPIO");
        gpiod_chip_close(chip);
        return 1;
    }

    // 4. Secuencia de leds (5 rondas)
    for (int ronda = 0; ronda < 5; ronda++) {
        for (int i = 0; i < N_LEDS; i++) {
            gpiod_line_request_set_value(request, pines[i], GPIOD_LINE_VALUE_ACTIVE);
            usleep(150000);
            gpiod_line_request_set_value(request, pines[i], GPIOD_LINE_VALUE_INACTIVE);
        }
    }

    // 5. Liberar recursos
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);

    return 0;
}
