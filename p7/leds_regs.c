#include <stdio.h>
#include <unistd.h>
#include "gpio_regs.h"
static const unsigned leds[4] = {17, 27, 22, 23};
#define LED_MASCARA (BIT(17) | BIT(27) | BIT(22) | BIT(23))
/* Cambia solo el campo de 3 bits de un pin: leer, borrar el campo, escribir */
static void configurar(gpio_regs_t *gpio, unsigned pin, uint32_t funcion)
{
unsigned reg = pin / 10;
unsigned desp = (pin % 10) * 3;
uint32_t valor = gpio->GPFSEL[reg];
valor &= ~(0x7u << desp);
valor |= (funcion & 0x7u) << desp;
gpio->GPFSEL[reg] = valor;
}
int main(void)
{
gpio_regs_t *gpio = gpio_abrir();
if (gpio == NULL)
return 1;
for (int i = 0; i < 4; i++)
configurar(gpio, leds[i], 1);
printf("Los cuatro LEDs con una sola escritura\n");
for (int ronda = 0; ronda < 3; ronda++) {
gpio->GPSET[0] = LED_MASCARA;
printf("GPLEV0 & mascara = 0x%08X\n", gpio->GPLEV[0] & LED_MASCARA);
usleep(400000);
gpio->GPCLR[0] = LED_MASCARA;
printf("GPLEV0 & mascara = 0x%08X\n", gpio->GPLEV[0] & LED_MASCARA);
usleep(400000);
}
printf("Un LED a la vez\n");
for (int ronda = 0; ronda < 3; ronda++) {
for (int i = 0; i < 4; i++) {
gpio->GPSET[0] = BIT(leds[i]);
usleep(150000);
gpio->GPCLR[0] = BIT(leds[i]);
}
}
for (int i = 0; i < 4; i++)
configurar(gpio, leds[i], 0);
return 0;
}