CC ?= gcc
CFLAGS = -std=gnu11 -Wall -Wextra -Wconversion -O2 -g
FUENTES = centinela_cfg.h centinela_logica.h hw.h

# Por omision: pruebas de logica (corren en cualquier computadora)
prueba: prueba_logica
	./prueba_logica || true

prueba_logica: prueba_logica.c centinela_cfg.h centinela_logica.h
	$(CC) $(CFLAGS) -o $@ $<

# Programa real: solo en la Raspberry Pi con el circuito
centinela: centinela.c $(FUENTES) hw_pi.h gpio_regs.h sensores_lib.h
	$(CC) $(CFLAGS) -o $@ $<

# Programa simulado: temperatura y botones de mentira, en cualquier computadora
sim: centinela.c $(FUENTES) hw_sim.h
	$(CC) $(CFLAGS) -DSIMULADO -o centinela_sim $<

clean:
	rm -f prueba_logica centinela centinela_sim centinela.csv

.PHONY: prueba sim clean
