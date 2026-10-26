CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2

all: radiografia prueba_logica

radiografia: radiografia.c radiografia_logica.h radiografia_cfg.h
	$(CC) $(CFLAGS) -o $@ radiografia.c

prueba_logica: prueba_logica.c radiografia_logica.h radiografia_cfg.h
	$(CC) $(CFLAGS) -o $@ prueba_logica.c

prueba: prueba_logica
	./prueba_logica || true

clean:
	rm -f radiografia prueba_logica
