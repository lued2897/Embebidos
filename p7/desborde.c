#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char *argv[])
{
int con_signo = (argc > 1) ? atoi(argv[1]) : INT_MAX - 2;
uint32_t sin_signo = (uint32_t)con_signo;
for (int i = 1; i <= 10; i++) {
con_signo++;
sin_signo++;
printf("%2d int: %11d uint32_t: %10" PRIu32 "\n",
i, con_signo, sin_signo);
}
return 0;
}