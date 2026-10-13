#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
struct paquete_normal {
uint8_t tipo;
uint32_t valor;
uint8_t crc;
};
struct __attribute__((packed)) paquete_compacto {
uint8_t tipo;
uint32_t valor;
uint8_t crc;
};
int main(void)
{
printf("--- tamaños en bytes ---\n");
printf("char %zu\n", sizeof(char));
printf("short %zu\n", sizeof(short));
printf("int %zu\n", sizeof(int));
printf("long %zu\n", sizeof(long));
printf("long long %zu\n", sizeof(long long));
printf("void * %zu\n", sizeof(void *));
printf("uint8_t %zu\n", sizeof(uint8_t));
printf("uint16_t %zu\n", sizeof(uint16_t));
printf("uint32_t %zu\n", sizeof(uint32_t));
printf("uint64_t %zu\n", sizeof(uint64_t));
printf("INT_MAX = %d, UINT32_MAX = %" PRIu32 "\n", INT_MAX, UINT32_MAX);
printf("\n--- promoción de enteros ---\n");
uint8_t a = 200, b = 100;
int amplia = a + b;
uint8_t recortada = (uint8_t)(a + b);
printf("a + b como int: %d, guardada en uint8_t: %u\n", amplia, recortada);
uint8_t reg = 0x0F;
printf("~reg = 0x%X, (uint8_t)~reg = 0x%X\n", ~reg, (uint8_t)~reg);
printf("~reg == 0xF0 es %d, (uint8_t)~reg == 0xF0 es %d\n",~reg == 0xF0, (uint8_t)~reg == 0xF0);
printf("\n--- relleno (padding) ---\n");
printf("normal: tamaño %zu, offset de valor %zu, offset de crc %zu\n",
sizeof(struct paquete_normal),
offsetof(struct paquete_normal, valor),
offsetof(struct paquete_normal, crc));
printf("compacto: tamaño %zu, offset de valor %zu, offset de crc %zu\n",
sizeof(struct paquete_compacto),
offsetof(struct paquete_compacto, valor),
offsetof(struct paquete_compacto, crc));
printf("\n--- orden de bytes ---\n");
uint32_t palabra = 0x01020304;
const uint8_t *bytes = (const uint8_t *)&palabra;
printf("0x01020304 en memoria: %02X %02X %02X %02X\n",
bytes[0], bytes[1], bytes[2], bytes[3]);
return 0;
}