#!/usr/bin/env python3
"""Proceso de ENTRADAS: huella (UART), teclado (GPIO), RFID (SPI) y reed switch (GPIO).

Cada sensor corre en su propio hilo. Las credenciales se mandan al proceso de
control por el socket UNIX; el estado de la puerta va por la cola de mensajes.

Uso:
    python3 entradas.py          # con hardware
    python3 entradas.py --sim    # sin hardware: se escriben eventos en consola
"""
import logging
import sys
import threading
import time

import config
import ipc

log = logging.getLogger("entradas")


def autenticar(metodo, valor, t_lectura=None):
    """Envía la credencial al control. t_lectura = momento en que el sensor la leyó
    (sirve para medir el requisito de apertura en menos de 3 s)."""
    r = ipc.enviar({"tipo": "auth", "metodo": metodo, "valor": valor,
                    "t": t_lectura or time.time()})
    log.info("%s -> %s", metodo, r)
    return r


def hilo(nombre, fn):
    """Corre fn y, si falla (sensor desconectado, etc.), la reintenta sin tumbar a los demás."""
    def run():
        while True:
            try:
                fn()
            except Exception as e:
                log.error("[%s] %s, reintentando en 3 s", nombre, e)
                time.sleep(3)
    t = threading.Thread(target=run, name=nombre, daemon=True)
    t.start()
    return t


# ---------------------------------------------------------------- Huella AS608
def tarea_huella():
    from pyfingerprint.pyfingerprint import PyFingerprint
    f = PyFingerprint(config.HUELLA_PUERTO, config.HUELLA_BAUD, 0xFFFFFFFF, 0x00000000)
    if not f.verifyPassword():
        raise RuntimeError("contraseña del sensor de huella incorrecta")
    log.info("huella lista (%d plantillas guardadas)", f.getTemplateCount())
    while True:
        if not f.readImage():
            time.sleep(0.1)
            continue
        t_lectura = time.time()
        f.convertImage(0x01)
        posicion, _score = f.searchTemplate()       # -1 si no coincide (búsqueda dentro del sensor)
        autenticar("huella", posicion, t_lectura)
        while f.readImage():                        # espera a que retire el dedo
            time.sleep(0.2)


# ---------------------------------------------------------------- RFID RC522
def tarea_rfid():
    from hw.rc522 import RC522
    lector = RC522(config.RFID_SPI_BUS, config.RFID_SPI_DEV)
    try:
        version = lector.version()
        if version in (0x00, 0xFF):
            raise RuntimeError(f"RC522 no responde (VersionReg=0x{version:02X})")
        log.info("RFID listo (VersionReg=0x%02X)", version)
        ultimo, t_ultimo, t_salud = None, 0.0, time.monotonic()
        while True:
            uid = lector.leer_uid()
            ahora = time.monotonic()
            if uid and not (uid == ultimo and ahora - t_ultimo < config.RFID_COOLDOWN_S):
                autenticar("rfid", uid, time.time())
                ultimo = uid
            if uid:
                t_ultimo = ahora
            # Detecta lector desconectado
            if ahora - t_salud > config.RFID_SALUD_S:
                t_salud = ahora
                version = lector.version()
                if version in (0x00, 0xFF):
                    raise RuntimeError(f"RC522 dejo de responder (VersionReg=0x{version:02X})")
            time.sleep(0.1)                         # bitácora clase 8: sondeo cada 100 ms
    finally:
        lector.close()


# ---------------------------------------------------------------- Teclado 4x4
def tarea_teclado():
    from gpiozero import InputDevice, OutputDevice
    filas = [OutputDevice(p, initial_value=True) for p in config.TECLADO_FILAS]
    cols = [InputDevice(p, pull_up=True) for p in config.TECLADO_COLS]

    def escanear():
        for i, fila in enumerate(filas):
            fila.off()                              # fila en bajo
            time.sleep(0.0002)
            for j, col in enumerate(cols):
                if col.is_active:                   # columna en bajo = tecla presionada
                    fila.on()
                    return config.TECLADO_MAPA[i][j]
            fila.on()
        return None

    try:
        _bucle_teclado(escanear)
    finally:                                        # libera los GPIO si el hilo falla
        for d in filas + cols:
            d.close()


def _bucle_teclado(escanear):
    log.info("teclado listo")
    pin, previa, t_tecla = "", None, time.monotonic()
    while True:
        tecla = escanear()
        if tecla and tecla != previa:
            t_tecla = time.monotonic()
            if tecla.isdigit() and len(pin) < config.PIN_MAX:
                pin += tecla
                ipc.enviar({"tipo": "pin_progreso", "n": len(pin)})
            elif tecla == "*":
                pin = ""
                ipc.enviar({"tipo": "pin_progreso", "n": 0})
            elif tecla == "#":
                if len(pin) >= config.PIN_MIN:
                    autenticar("pin", pin, time.time())
                else:
                    ipc.enviar({"tipo": "pin_progreso", "n": 0})
                pin = ""
        if pin and time.monotonic() - t_tecla > config.PIN_TIMEOUT_S:
            pin = ""
            ipc.enviar({"tipo": "pin_progreso", "n": 0})
        previa = tecla
        time.sleep(0.02)                            # bitácora clase 8: barrido a 50 Hz (antirrebote)


# ---------------------------------------------------------------- Reed switch MC-38
def tarea_reed():
    from gpiozero import Button
    cola = ipc.cola_puerta()
    reed = Button(config.REED_GPIO, pull_up=True, bounce_time=0.05)   # imán cerca = cerrada

    def publicar(estado):
        try:
            cola.send(estado, timeout=0)
        except Exception:
            pass                                    # cola llena: control no está leyendo
        log.info("puerta %s", estado)

    try:
        reed.when_pressed = lambda: publicar("cerrada")
        reed.when_released = lambda: publicar("abierta")
        publicar("cerrada" if reed.is_pressed else "abierta")
        log.info("reed switch listo")
        threading.Event().wait()                    # los callbacks hacen el trabajo
    finally:
        reed.close()                                # libera GPIO17 si el hilo falla


# ---------------------------------------------------------------- Simulación
def modo_simulacion():
    cola = ipc.cola_puerta()
    print("Comandos: rfid <UID> | pin <1234> | huella <id> | puerta abierta|cerrada | salir")
    for linea in sys.stdin:
        partes = linea.split()
        if not partes:
            continue
        if partes[0] == "salir":
            break
        if partes[0] == "puerta" and len(partes) == 2:
            cola.send(partes[1])
        elif partes[0] in ("rfid", "pin", "huella") and len(partes) == 2:
            autenticar(partes[0], partes[1])
        else:
            print("comando no reconocido")


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    if "--sim" in sys.argv:
        modo_simulacion()
        return
    try:
        from gpiozero import Device
        from gpiozero.pins.lgpio import LGPIOFactory
        Device.pin_factory = LGPIOFactory()         # acceso a GPIO por /dev/gpiochip (libgpiod/lgpio)
    except Exception as e:
        log.warning("no se pudo usar lgpio: %s", e)
    if config.HUELLA_ACTIVA:
        hilo("huella", tarea_huella)
    else:
        log.info("huella desactivada en config.py")
    hilo("rfid", tarea_rfid)
    hilo("teclado", tarea_teclado)
    hilo("reed", tarea_reed)
    threading.Event().wait()


if __name__ == "__main__":
    main()
