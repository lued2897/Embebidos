#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#define CHIP "/dev/gpiochip0"

int main(void) {
    unsigned int pins[] = {5, 17};

    struct gpiod_chip *chip = gpiod_chip_open(CHIP);
    if (!chip) {
        perror("GPIO");
        return 1;
    }

    struct gpiod_line_settings *in = gpiod_line_settings_new();
    struct gpiod_line_settings *out = gpiod_line_settings_new();

    gpiod_line_settings_set_direction(in, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_direction(out, GPIOD_LINE_DIRECTION_OUTPUT);

    struct gpiod_line_config *cfg = gpiod_line_config_new();

    gpiod_line_config_add_line_settings(cfg, &pins[0], 1, in);
    gpiod_line_config_add_line_settings(cfg, &pins[1], 1, out);

    struct gpiod_request_config *req = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req, "boton_polling");

    struct gpiod_line_request *lines =
        gpiod_chip_request_lines(chip, req, cfg);

    while (1) {
        int estado = gpiod_line_request_get_value(lines, pins[0]);
        gpiod_line_request_set_value(lines, pins[1], estado);
        usleep(10000);
    }
}
