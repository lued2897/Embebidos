#include <stdio.h>
#include "gpio_regs.h"
static const char *nombre_funcion(uint32_t f)
{
switch (f) {
case 0: return "entrada";
case 1: return "salida";
case 4: return "ALT0";
case 5: return "ALT1";
case 6: return "ALT2";
case 7: return "ALT3";
case 3: return "ALT4";
case 2: return "ALT5";
default: return "?";
}
}
int main(void)
{
const unsigned pines[] = {2, 3, 8, 9, 10, 11, 5, 6, 17, 27, 22, 23};
gpio_regs_t *gpio = gpio_abrir();
if (gpio == NULL)
return 1;
printf("pin GPFSEL desplazamiento campo funcion nivel\n");
for (size_t i = 0; i < sizeof(pines) / sizeof(pines[0]); i++) {
unsigned pin = pines[i];
unsigned reg = pin / 10;
unsigned desp = (pin % 10) * 3;
uint32_t campo = (gpio->GPFSEL[reg] >> desp) & 0x7u;
unsigned nivel = (gpio->GPLEV[0] >> pin) & 1u;
printf("%3u %6u %14u %5u %-8s %5u\n",
pin, reg, desp, campo, nombre_funcion(campo), nivel);
}
return 0;
}