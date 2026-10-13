#include <stdio.h>
#include "gpio_regs.h"
#define BOTON 5
int main(void)
{
gpio_regs_t *gpio = gpio_abrir();
if (gpio == NULL)
return 1;
/* GPIO5 como entrada: campo de 3 bits en cero */
gpio->GPFSEL[BOTON / 10] &= ~(0x7u << ((BOTON % 10) * 3));
printf("Presiona el boton de GPIO%d (maximo 10 s)\n", BOTON);
while (!(gpio->GPLEV[0] & BIT(BOTON)))
;
printf("Boton detectado\n");
return 0;
}