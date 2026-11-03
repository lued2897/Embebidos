#!/usr/bin/env python3
"""Proceso de CONTROL: decide y acciona.

- Único proceso que escribe al PCA9685 (servo). Entradas y web le piden la
  apertura por el socket UNIX y él las atiende una por una (FIFO).
- Un hilo lee la cola de mensajes del reed switch; un mutex/condición protege
  la variable de estado de la puerta, compartida con el hilo del socket.
- Registra todo en la base de datos (fecha, método, resultado, usuario).

Uso:
    python3 control.py           # con hardware
    python3 control.py --sim     # sin servo ni LCD (imprime en consola)
"""
import logging
import os
import signal
import socket
import sys
import threading
import time

import config
import db
import ipc

log = logging.getLogger("control")


# ---------------------------------------------------------------- Hardware simulado
class ServoSim:
    def set_pwm(self, canal, cuentas):
        log.info("[SERVO] canal %d -> %d cuentas (%.2f ms)", canal, cuentas, cuentas * 20 / 4096)


class LCDSim:
    def mostrar(self, *lineas):
        print("+" + "-" * 20 + "+")
        for i in range(4):
            print("|" + (lineas[i] if i < len(lineas) else "").ljust(20) + "|")
        print("+" + "-" * 20 + "+", flush=True)


# ---------------------------------------------------------------- Control
class Control:
    def __init__(self, sim=False):
        self.con = db.conectar()
        if sim:
            self.servo, self.lcd = ServoSim(), LCDSim()
        else:
            from hw.pca9685 import PCA9685
            from hw.lcd import LCD
            self.servo = PCA9685(config.I2C_BUS, config.PCA9685_ADDR)
            try:
                self.lcd = LCD(config.I2C_BUS, config.LCD_ADDR, config.LCD_COLS, config.LCD_FILAS)
            except OSError as e:
                log.error("LCD no encontrado (%s), sigo sin display", e)
                self.lcd = LCDSim()

        self.mutex = threading.Lock()                 # protege estado_puerta
        self.cambio_puerta = threading.Condition(self.mutex)
        self.estado_puerta = "desconocido"
        self.servo_lock = threading.Lock()
        self.desbloqueada = False
        self.fallos = 0
        self.bloqueado_hasta = 0.0
        self._gen_pantalla = 0

        self._mover(abrir=False)
        self.pantalla_espera()

    # ------------------------------------------------ actuadores
    def _mover(self, abrir):
        with self.servo_lock:
            self.servo.set_pwm(config.SERVO_CANAL, config.SERVO_ABRIR if abrir else config.SERVO_CERRAR)

    def pantalla_espera(self):
        with self.mutex:
            puerta = self.estado_puerta
        self.lcd.mostrar(" CERRADURA EQUIPO 8", "Acerque tarjeta,", "huella o teclee PIN", f"Puerta: {puerta}")

    def mensaje(self, *lineas, duracion=3.0):
        """Muestra un mensaje temporal y luego vuelve a la pantalla de espera."""
        self._gen_pantalla += 1
        gen = self._gen_pantalla
        self.lcd.mostrar(*lineas)

        def volver():
            time.sleep(duracion)
            if gen == self._gen_pantalla:
                self.pantalla_espera()
        threading.Thread(target=volver, daemon=True).start()

    # ------------------------------------------------ apertura
    def abrir(self, usuario, metodo):
        if not self.desbloqueada:
            self.desbloqueada = True
            self._mover(abrir=True)
            threading.Thread(target=self._volver_a_cerrar, daemon=True).start()
        db.log(self.con, metodo, "concedido", usuario)
        self.mensaje("ACCESO CONCEDIDO", f"Hola, {usuario}", "", "Puede pasar")

    def _volver_a_cerrar(self):
        with self.cambio_puerta:
            abrio = self.cambio_puerta.wait_for(
                lambda: self.estado_puerta == "abierta", timeout=config.TIEMPO_ABIERTO_S)
            if abrio:                                   # espera a que la cierren
                self.cambio_puerta.wait_for(lambda: self.estado_puerta == "cerrada")
        if abrio:
            time.sleep(1)
        self._mover(abrir=False)
        self.desbloqueada = False
        db.log(self.con, "sistema", "cerradura bloqueada", detalle="puerta usada" if abrio else "tiempo agotado")
        log.info("cerradura bloqueada")

    # ------------------------------------------------ autenticación
    def autenticar(self, metodo, valor):
        ahora = time.monotonic()
        if ahora < self.bloqueado_hasta:
            resta = int(self.bloqueado_hasta - ahora) + 1
            db.log(self.con, metodo, "denegado", detalle="sistema bloqueado")
            self.mensaje("SISTEMA BLOQUEADO", f"Espere {resta} s", duracion=2)
            return {"ok": False, "motivo": "bloqueado", "segundos": resta}

        usuario = db.buscar_por(self.con, metodo, valor)
        if usuario:
            self.fallos = 0
            self.abrir(usuario, metodo)
            return {"ok": True, "usuario": usuario}

        self.fallos += 1
        db.log(self.con, metodo, "denegado", detalle=f"credencial no registrada (fallo {self.fallos})")
        if self.fallos >= config.MAX_INTENTOS:
            self.fallos = 0
            self.bloqueado_hasta = ahora + config.BLOQUEO_S
            db.log(self.con, "sistema", "bloqueo temporal", detalle=f"{config.MAX_INTENTOS} intentos fallidos")
            self.mensaje("SISTEMA BLOQUEADO", f"Demasiados intentos", f"Espere {config.BLOQUEO_S} s",
                         duracion=config.BLOQUEO_S)
            return {"ok": False, "motivo": "bloqueado", "segundos": config.BLOQUEO_S}
        self.mensaje("ACCESO DENEGADO", "Credencial invalida", "",
                     f"Intentos: {self.fallos}/{config.MAX_INTENTOS}")
        return {"ok": False, "motivo": "denegado", "fallos": self.fallos}

    # ------------------------------------------------ mensajes del socket
    def atender(self, msg):
        tipo = msg.get("tipo")
        if tipo == "auth":
            return self.autenticar(msg.get("metodo"), msg.get("valor"))
        if tipo == "pin_progreso":
            n = int(msg.get("n", 0))
            if n:
                self.mensaje("Ingrese su PIN:", "", "*" * n, "# = OK   * = borrar", duracion=config.PIN_TIMEOUT_S)
            else:
                self.pantalla_espera()
            return {"ok": True}
        if tipo == "abrir_remoto":                      # lo usará el proceso web
            self.abrir(msg.get("usuario", "admin"), "remoto")
            return {"ok": True}
        if tipo == "estado":
            with self.mutex:
                puerta = self.estado_puerta
            return {"ok": True, "puerta": puerta, "desbloqueada": self.desbloqueada,
                    "bloqueado": time.monotonic() < self.bloqueado_hasta}
        return {"ok": False, "error": f"tipo desconocido: {tipo}"}

    # ------------------------------------------------ hilo del reed switch
    def hilo_puerta(self):
        cola = ipc.cola_puerta()
        while True:
            msg, _ = cola.receive()
            estado = msg.decode()
            with self.cambio_puerta:
                previo = self.estado_puerta
                self.estado_puerta = estado
                self.cambio_puerta.notify_all()
            if estado == previo:
                continue
            forzada = estado == "abierta" and not self.desbloqueada
            db.log(self.con, "puerta", estado, detalle="ALERTA: abierta sin autorizacion" if forzada else None)
            log.info("puerta %s%s", estado, " (sin autorizacion)" if forzada else "")
            if forzada:
                self.mensaje("ALERTA", "Puerta abierta sin", "autorizacion", duracion=5)
            else:
                self.lcd_linea_puerta()

    def lcd_linea_puerta(self):
        if not hasattr(self.lcd, "linea"):
            return self.pantalla_espera()
        with self.mutex:
            puerta = self.estado_puerta
        self.lcd.linea(3, f"Puerta: {puerta}")

    # ------------------------------------------------ servidor
    def servir(self):
        threading.Thread(target=self.hilo_puerta, name="reed", daemon=True).start()
        if os.path.exists(config.SOCKET_PATH):
            os.unlink(config.SOCKET_PATH)
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(config.SOCKET_PATH)
        os.chmod(config.SOCKET_PATH, 0o660)
        srv.listen(16)
        log.info("control escuchando en %s", config.SOCKET_PATH)
        try:
            while True:
                conn, _ = srv.accept()                  # una solicitud a la vez
                with conn:
                    conn.settimeout(2)
                    t0 = time.perf_counter()
                    try:
                        resp = self.atender(ipc.leer_mensaje(conn))
                    except Exception as e:
                        log.exception("error atendiendo mensaje")
                        resp = {"ok": False, "error": str(e)}
                    resp["ms"] = round((time.perf_counter() - t0) * 1000, 1)
                    try:
                        ipc.responder(conn, resp)
                    except OSError:
                        pass
        finally:
            srv.close()
            if os.path.exists(config.SOCKET_PATH):
                os.unlink(config.SOCKET_PATH)


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    Control(sim="--sim" in sys.argv).servir()


if __name__ == "__main__":
    main()
