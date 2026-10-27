# Practica 9. Compila todos los programas con informacion de depuracion y sin optimizar.
CC = gcc
CFLAGS = -std=gnu11 -g -O0 -Wall -Wextra
PROGRAMAS = estadistica f1_nulo f2_bucle f3_corrupcion averiado

all: $(PROGRAMAS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGRAMAS)
