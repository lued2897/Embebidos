#ifndef HW_H
#define HW_H

/* Capa de hardware del Centinela. No se modifica.
 *   hw_pi.h  : circuito real (registros del GPIO, I2C y SPI), como en la Practica 7
 *   hw_sim.h : circuito simulado, para probar en cualquier computadora (make sim)
 * Funciones comunes:
 *   int      hw_abrir(hw_t *h, int direccion_bmp);   0 si todo va bien
 *   int      hw_temp(hw_t *h, int32_t *temp_c100);   0 si leyo la temperatura
 *   int      hw_pot(hw_t *h, unsigned *valor);       0 si leyo el potenciometro (0 a 1023)
 *   unsigned hw_botones(hw_t *h);                    bit 0 = GPIO5, bit 1 = GPIO6 (1 = presionado)
 *   void     hw_leds(hw_t *h, uint32_t mascara);     enciende los LEDs de la mascara y apaga los demas
 *   void     hw_cerrar(hw_t *h);                     apaga los LEDs y devuelve sus pines a entrada
 */
#ifdef SIMULADO
#include "hw_sim.h"
#else
#include "hw_pi.h"
#endif

#endif
