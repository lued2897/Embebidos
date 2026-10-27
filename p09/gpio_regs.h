#ifndef GPIO_REGS_H
#define GPIO_REGS_H

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef BIT
#define BIT(n) ((uint32_t)1u << (n))
#endif

#ifdef SIN_VOLATILE
#define VOL
#else
#define VOL volatile
#endif

/* Mismo orden y desplazamientos que el controlador GPIO de la Pi 4 */
typedef struct {
	VOL uint32_t GPFSEL[6];  /* 0x00 a 0x14 */
	uint32_t     reservado0; /* 0x18 */
	VOL uint32_t GPSET[2];   /* 0x1C a 0x20 */
	uint32_t     reservado1; /* 0x24 */
	VOL uint32_t GPCLR[2];   /* 0x28 a 0x2C */
	uint32_t     reservado2; /* 0x30 */
	VOL uint32_t GPLEV[2];   /* 0x34 a 0x38 */
} gpio_regs_t;

_Static_assert(offsetof(gpio_regs_t, GPFSEL) == 0x00, "offset de GPFSEL0");
_Static_assert(offsetof(gpio_regs_t, GPSET)  == 0x1C, "offset de GPSET0");
_Static_assert(offsetof(gpio_regs_t, GPCLR)  == 0x28, "offset de GPCLR0");
_Static_assert(offsetof(gpio_regs_t, GPLEV)  == 0x34, "offset de GPLEV0");

/* Proyecta los registros reales del GPIO en la memoria del programa */
static inline gpio_regs_t *gpio_abrir(void)
{
	int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);

	if (fd < 0) {
		perror("open /dev/gpiomem");
		return NULL;
	}
	void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	close(fd);
	if (p == MAP_FAILED) {
		perror("mmap");
		return NULL;
	}
	return (gpio_regs_t *)p;
}

#endif
