#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
const char *msg = (argc > 1) ? argv[1] : "hola desde espacio de usuario";
char recibido[256];
int fd = open("/dev/p6buf", O_RDWR);
if (fd < 0) {
perror("open /dev/p6buf");
return 1;
}
if (write(fd, msg, strlen(msg)) < 0) {
perror("write");
close(fd);
return 1;
}
lseek(fd, 0, SEEK_SET);
ssize_t n = read(fd, recibido, sizeof(recibido) - 1);
if (n < 0) {
perror("read");
close(fd);
return 1;
}
recibido[n] = '\0';
printf("el driver devolvio %zd bytes: %s\n", n, recibido);
close(fd);
return 0;
}