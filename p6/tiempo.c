#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#define N 1000000
#define N_SPI 10000
static double ahora_ns(void)
{
struct timespec t;
clock_gettime(CLOCK_MONOTONIC, &t);
return t.tv_sec * 1e9 + t.tv_nsec;
}
int main(void)
{
static volatile char memoria = 'x';
char c = 0;
double t0, t1, t2, t3, t4, t5;
int fd = open("/dev/p6buf", O_RDONLY);
if (fd < 0) {
perror("open /dev/p6buf");
return 1;
}
t0 = ahora_ns();
for (int i = 0; i < N; i++)
c = memoria;
t1 = ahora_ns();
for (int i = 0; i < N; i++)
syscall(SYS_getppid);
t2 = ahora_ns();
for (int i = 0; i < N; i++) {
if (pread(fd, &c, 1, 0) < 0) {
perror("pread");
return 1;
}
}
t3 = ahora_ns();
printf("lectura de memoria de usuario: %8.1f ns por operacion\n", (t1 - t0) / N);
printf("syscall getppid: %8.1f ns por operacion\n", (t2 - t1) / N);
printf("pread sobre /dev/p6buf: %8.1f ns por operacion\n", (t3 - t2) / N);
(void)c;
close(fd);
int fs = open("/dev/spidev0.0", O_RDWR);
if (fs < 0) {
printf("spidev no disponible: se omite la medicion SPI\n");
return 0;
}
unsigned char tx[3] = {1, 8 << 4, 0};
unsigned char rx[3] = {0};
struct spi_ioc_transfer tr = {
.tx_buf = (unsigned long)tx,
.rx_buf = (unsigned long)rx,
.len = 3,
.speed_hz = 1000000,
.bits_per_word = 8,
};
t4 = ahora_ns();
for (int i = 0; i < N_SPI; i++) {
if (ioctl(fs, SPI_IOC_MESSAGE(1), &tr) < 0) {
perror("ioctl SPI_IOC_MESSAGE");
break;
}
}
t5 = ahora_ns();
printf("transferencia SPI de 3 bytes: %8.1f ns por operacion\n", (t5 - t4) / N_SPI);
close(fs);
return 0;
}