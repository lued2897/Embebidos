"""Driver mínimo para LCD 20x4 HD44780 con módulo I2C PCF8574.

Las líneas de más de 20 caracteres se desplazan solas (scroll tipo marquesina)
en un hilo de baja prioridad.
"""
import os
import threading
import time
from smbus2 import SMBus

RS, EN, BL = 0x01, 0x04, 0x08
OFFSETS_FILA = [0x00, 0x40, 0x14, 0x54]

SCROLL_PASO_S = 0.35       # tiempo entre cada desplazamiento
SCROLL_PAUSA = 4           # pasos de espera al inicio del texto antes de moverse
SCROLL_SEPARADOR = "    "  # espacio entre el final y el reinicio del texto


class LCD:
    def __init__(self, bus=1, addr=0x27, cols=20, filas=4, nice_scroll=10):
        self.nice_scroll = nice_scroll
        self.bus = SMBus(bus)
        self.addr, self.cols, self.filas = addr, cols, filas
        self.lock = threading.Lock()
        self._texto = [""] * filas         # texto completo de cada fila
        self._pos = [0] * filas            # posición del scroll en cada fila
        time.sleep(0.05)
        for _ in range(3):
            self._write4(0x30)
            time.sleep(0.005)
        self._write4(0x20)               # modo 4 bits
        self._cmd(0x28)                  # 2+ líneas, 5x8
        self._cmd(0x0C)                  # display on, sin cursor
        self._cmd(0x06)                  # incremento automático
        self.limpiar()
        threading.Thread(target=self._hilo_scroll, name="lcd-scroll", daemon=True).start()

    # ------------------------------------------------ bajo nivel
    def _write4(self, data):
        self.bus.write_byte(self.addr, data | BL)
        self.bus.write_byte(self.addr, data | EN | BL)
        time.sleep(0.0005)
        self.bus.write_byte(self.addr, (data & ~EN) | BL)
        time.sleep(0.0001)

    def _send(self, valor, modo):
        self._write4(modo | (valor & 0xF0))
        self._write4(modo | ((valor << 4) & 0xF0))

    def _cmd(self, c):
        self._send(c, 0)

    def _escribir_fila(self, fila, visible):
        """Escribe exactamente cols caracteres en la fila (llamar con self.lock tomado)."""
        self._cmd(0x80 | OFFSETS_FILA[fila])
        for ch in visible.ljust(self.cols)[: self.cols]:
            self._send(ord(ch) if ord(ch) < 128 else ord("?"), RS)

    def _ventana(self, fila):
        texto = self._texto[fila]
        if len(texto) <= self.cols:
            return texto
        ciclo = texto + SCROLL_SEPARADOR
        inicio = max(0, self._pos[fila] - SCROLL_PAUSA) % len(ciclo)
        return (ciclo + ciclo)[inicio: inicio + self.cols]

    def _hilo_scroll(self):
        # Bitácora clase 8: el display es de prioridad baja. En Linux el nice es por
        # hilo, así que este hilo baja su prioridad sin afectar al resto del proceso.
        try:
            os.setpriority(os.PRIO_PROCESS, threading.get_native_id(), self.nice_scroll)
        except OSError:
            pass
        while True:
            time.sleep(SCROLL_PASO_S)
            with self.lock:
                for f in range(self.filas):
                    if len(self._texto[f]) > self.cols:
                        self._pos[f] += 1
                        largo = len(self._texto[f]) + len(SCROLL_SEPARADOR)
                        if self._pos[f] - SCROLL_PAUSA >= largo:
                            self._pos[f] = 0          # vuelve al inicio y hace la pausa
                        self._escribir_fila(f, self._ventana(f))

    # ------------------------------------------------ API
    def limpiar(self):
        with self.lock:
            self._texto = [""] * self.filas
            self._pos = [0] * self.filas
            self._cmd(0x01)
            time.sleep(0.002)

    def linea(self, fila, texto):
        with self.lock:
            if texto == self._texto[fila]:
                return                                # sin cambios: no reinicia el scroll
            self._texto[fila] = texto
            self._pos[fila] = 0
            self._escribir_fila(fila, self._ventana(fila))

    def mostrar(self, *lineas):
        for i in range(self.filas):
            self.linea(i, lineas[i] if i < len(lineas) else "")

    def close(self):
        self.bus.close()
