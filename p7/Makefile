CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -O0 -g
PROGS = tipos bits desborde regs_leer leds_regs sensores

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

regs_leer leds_regs: gpio_regs.h

clean:
	rm -f $(PROGS)