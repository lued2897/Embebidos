#include <stdint.h>
#include <stdio.h>
#define BIT(n) ((uint32_t)1u << (n))
#define SET_BIT(r, n) ((r) |= BIT(n))
#define CLEAR_BIT(r, n) ((r) &= ~BIT(n))
#define TOGGLE_BIT(r, n) ((r) ^= BIT(n))
#define TEST_BIT(r, n) (((r) >> (n)) & 1u)
static void imprimir(const char *etiqueta, uint32_t v)
{
printf("%-22s 0x%08X ", etiqueta, v);
for (int i = 31; i >= 0; i--) {
putchar(((v >> i) & 1u) ? '1' : '0');
if (i % 8 == 0 && i != 0)
putchar('_');
}
putchar('\n');
}
int main(void)
{
uint32_t r = 0;
imprimir("inicio", r);
SET_BIT(r, 17);
imprimir("SET_BIT(r, 17)", r);
SET_BIT(r, 3);
imprimir("SET_BIT(r, 3)", r);
TOGGLE_BIT(r, 17);
imprimir("TOGGLE_BIT(r, 17)", r);
CLEAR_BIT(r, 3);
imprimir("CLEAR_BIT(r, 3)", r);
printf("TEST_BIT(r, 17) = %u\n", TEST_BIT(r, 17));
printf("\n--- campo de 3 bits en GPFSEL1 (GPIO17: bits 23:21) ---\n");
uint32_t gpfsel1 = 0xFFFFFFFFu;
uint32_t desplazamiento = 6;
uint32_t mascara = 0x7u << desplazamiento;
imprimir("GPFSEL1 inicial", gpfsel1);
imprimir("máscara del campo", mascara);
gpfsel1 = (gpfsel1 & ~mascara) | (1u << desplazamiento);
imprimir("GPIO17 como salida", gpfsel1);
printf("campo leído de vuelta: %u\n", (gpfsel1 >> desplazamiento) & 0x7u);
return 0;
}