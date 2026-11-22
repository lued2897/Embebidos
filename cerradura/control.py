#!/usr/bin/env python3
"""Proceso de CONTROL: decide y acciona.

- Único proceso que escribe al PCA9685 (servo). Entradas y web le piden la
  apertura por el socket UNIX y él las atiende una por una (FIFO).
- Un hilo lee la cola de mensajes del reed switch; un mutex/condición protege
  la variable de estado de la puerta, compartida con el hilo del socket.
- Registra todo en la base de datos (fecha, método, resultado, usuario).
- Atiende el alta de tarjeta y huella pedida desde la web: la siguiente tarjeta
  leída se guarda en vez de abrir, y la huella se pide al proceso de entradas
  por la cola /cerradura_ordenes.

Uso:
    python3 control.py           # con hardware
    python3 control.py --sim     # sin servo ni LCD (imprime en consola)
"""
import logging
import os
import signal
import socket
import sqlite3
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
                self.lcd = LCD(config.I2C_BUS, config.LCD_ADDR, config.LCD_COLS, config.LCD_FILAS,
                               nice_scroll=config.NICE_HILO_LCD)
            except OSError as e:
                log.error("LCD no encontrado (%s), sigo sin display", e)
                self.lcd = LCDSim()

        # Bitácora clase 9: un mutex protege el estado de la puerta, que comparten
        # el hilo del socket, el hilo del reed y el hilo que vuelve a cerrar.
        self.mutex = threading.Lock()
        self.cambio_puerta = threading.Condition(self.mutex)
        self.estado_puerta = "desconocido"
        # Bitácora clase 9: el servo es el recurso crítico. Un solo candado cubre la
        # escritura al PCA9685 y la bandera "desbloqueada", para que abrir y volver a
        # cerrar nunca se intercalen.
        self.servo_lock = threading.RLock()
        self.desbloqueada = False
        self.fallos = 0
        self.bloqueado_hasta = 0.0
        # Registro de tarjeta o huella pedido desde la web. Solo lo usa el hilo del
        # socket, que atiende una solicitud a la vez: no necesita candado.
        self.registro = None
        self._n_registro = 0
        self._gen_pantalla = 0

        self._mover(abrir=False)
        self.pantalla_espera()

    # ------------------------------------------------ actuadores
    def _mover(self, abrir):
        # Bitácora clase 9 y 12: solo este proceso escribe al driver I2C del servo.
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
    def abrir(self, usuario, metodo, t_lectura=None):
        """Abre el servo. t_lectura = time.time() del momento en que se leyó la credencial."""
        detalle = None
        with self.servo_lock:
            if not self.desbloqueada:
                self.desbloqueada = True
                self._mover(abrir=True)
                if t_lectura:
                    # Medición del requisito "apertura en menos de 3 s" (Avance 1):
                    # desde que el sensor leyó la credencial hasta la orden al servo.
                    detalle = f"latencia {(time.time() - t_lectura) * 1000:.1f} ms"
                threading.Thread(target=self._volver_a_cerrar, name="cierre", daemon=True).start()
            else:
                detalle = "ya estaba abierta"
        db.log(self.con, metodo, "concedido", usuario, detalle)
        self.mensaje("ACCESO CONCEDIDO", f"Hola, {usuario}", "", "Puede pasar")
        return detalle

    def _volver_a_cerrar(self):
        with self.cambio_puerta:
            abrio = self.cambio_puerta.wait_for(
                lambda: self.estado_puerta == "abierta", timeout=config.TIEMPO_ABIERTO_S)
            if abrio:                                   # espera a que la cierren
                self.cambio_puerta.wait_for(lambda: self.estado_puerta == "cerrada")
        if abrio:
            time.sleep(1)
        with self.servo_lock:
            self._mover(abrir=False)
            self.desbloqueada = False
        db.log(self.con, "sistema", "cerradura bloqueada", detalle="puerta usada" if abrio else "tiempo agotado")
        log.info("cerradura bloqueada")

    # ------------------------------------------------ alta y baja desde la web
    def registro_activo(self):
        r = self.registro
        if r and r["estado"] == "esperando" and time.time() > r["vence"]:
            r.update(estado="error", mensaje="tiempo agotado")
            self.pantalla_espera()
        return bool(r) and r["estado"] == "esperando"

    def iniciar_registro(self, usuario, metodo):
        if metodo not in ("tarjeta", "huella"):
            return {"ok": False, "error": f"método desconocido: {metodo}"}
        if not db.usuario(self.con, usuario):
            return {"ok": False, "error": "el usuario no existe"}
        if self.registro_activo():
            return {"ok": False, "error": "ya hay un registro en curso, espere a que termine"}
        self._n_registro += 1
        r = {"id": self._n_registro, "usuario": usuario, "metodo": metodo, "estado": "esperando",
             "mensaje": "", "valor": None, "vence": time.time() + config.REGISTRO_TIMEOUT_S}
        if metodo == "huella":
            try:                                        # el AS608 lo maneja el proceso de entradas
                ipc.enviar_orden({"orden": "huella_registrar", "id": r["id"], "vence": r["vence"]})
            except Exception as e:
                return {"ok": False, "error": f"no se pudo avisar al sensor de huella: {e}"}
            self.mensaje("REGISTRO DE HUELLA", f"Usuario: {usuario}", "Coloque el dedo",
                         duracion=config.REGISTRO_TIMEOUT_S)
        else:
            self.mensaje("REGISTRO DE TARJETA", f"Usuario: {usuario}", "Acerque la tarjeta",
                         duracion=config.REGISTRO_TIMEOUT_S)
        self.registro = r
        return {"ok": True, "id": r["id"], "segundos": config.REGISTRO_TIMEOUT_S}

    def guardar_credencial(self, campo, valor):
        r = self.registro
        anterior = db.usuario(self.con, r["usuario"])
        try:
            if not anterior or not db.asignar(self.con, r["usuario"], campo, valor):
                return self.fallar_registro("el usuario ya no existe")
        except sqlite3.IntegrityError:
            return self.fallar_registro("esa tarjeta ya es de otro usuario" if campo == "rfid_uid"
                                        else "esa huella ya es de otro usuario")
        if campo == "huella_id" and anterior["huella_id"] not in (None, valor):
            self.borrar_plantilla(anterior["huella_id"], r["usuario"])     # la huella anterior ya no sirve
        que = "tarjeta" if campo == "rfid_uid" else "huella"
        r.update(estado="ok", mensaje=f"{que} registrada", valor=str(valor))
        db.log(self.con, "web", f"{que} registrada", r["usuario"], f"{campo} {valor}")
        self.mensaje("REGISTRO EXITOSO", f"{que.capitalize()} guardada", f"Usuario: {r['usuario']}")
        return {"ok": True, "registro": True, "usuario": r["usuario"]}

    def fallar_registro(self, mensaje):
        self.registro.update(estado="error", mensaje=mensaje)
        self.mensaje("REGISTRO FALLIDO", mensaje, duracion=4)
        return {"ok": False, "motivo": "registro", "mensaje": mensaje}

    def resultado_huella(self, msg):
        r = self.registro
        if not (r and r["id"] == msg.get("id") and self.registro_activo()):
            # llegó tarde: si el sensor sí guardó la plantilla, se borra para no dejarla huérfana
            if msg.get("ok") and msg.get("huella_id") is not None:
                self.borrar_plantilla(msg["huella_id"], "registro vencido")
            return {"ok": False, "error": "registro vencido"}
        if not msg.get("ok"):
            return self.fallar_registro(msg.get("mensaje", "error del sensor"))
        resp = self.guardar_credencial("huella_id", int(msg["huella_id"]))
        if not resp["ok"]:                              # no se pudo asignar: no dejarla en el sensor
            self.borrar_plantilla(msg["huella_id"], r["usuario"])
        return resp

    def borrar_plantilla(self, huella_id, usuario):
        try:
            ipc.enviar_orden({"orden": "huella_borrar", "huella_id": huella_id})
        except Exception as e:
            db.alerta(self.con, "huella", f"no se pudo borrar la plantilla {huella_id} ({usuario}): {e}", "aviso")

    def baja(self, usuario, admin):
        fila = db.usuario(self.con, usuario)
        if not fila:
            return {"ok": False, "error": "el usuario no existe"}
        db.eliminar_usuario(self.con, usuario)
        if fila["huella_id"] is not None:
            self.borrar_plantilla(fila["huella_id"], usuario)    # también sale del AS608
        db.log(self.con, "web", "usuario eliminado", admin, usuario)
        return {"ok": True}

    # ------------------------------------------------ autenticación
    def autenticar(self, metodo, valor, t_lectura=None):
        if metodo == "rfid" and self.registro_activo() and self.registro["metodo"] == "tarjeta":
            return self.guardar_credencial("rfid_uid", valor)   # se registra, no abre la puerta
        ahora = time.monotonic()
        if ahora < self.bloqueado_hasta:
            resta = int(self.bloqueado_hasta - ahora) + 1
            db.log(self.con, metodo, "denegado", detalle="sistema bloqueado")
            self.mensaje("SISTEMA BLOQUEADO", f"Espere {resta} s", duracion=2)
            return {"ok": False, "motivo": "bloqueado", "segundos": resta}

        usuario = db.buscar_por(self.con, metodo, valor)
        if usuario:
            self.fallos = 0
            detalle = self.abrir(usuario, metodo, t_lectura)
            return {"ok": True, "usuario": usuario, "detalle": detalle}

        self.fallos += 1
        db.log(self.con, metodo, "denegado", detalle=f"credencial no registrada (fallo {self.fallos})")
        if self.fallos >= config.MAX_INTENTOS:
            self.fallos = 0
            self.bloqueado_hasta = ahora + config.BLOQUEO_S
            db.alerta(self.con, "sistema", f"bloqueo temporal: {config.MAX_INTENTOS} intentos fallidos seguidos",
                      "critica")
            self.mensaje("SISTEMA BLOQUEADO", "Demasiados intentos", f"Espere {config.BLOQUEO_S} s",
                         duracion=config.BLOQUEO_S)
            return {"ok": False, "motivo": "bloqueado", "segundos": config.BLOQUEO_S}
        self.mensaje("ACCESO DENEGADO", "Credencial invalida", "",
                     f"Intentos: {self.fallos}/{config.MAX_INTENTOS}")
        return {"ok": False, "motivo": "denegado", "fallos": self.fallos}

    # ------------------------------------------------ mensajes del socket
    def atender(self, msg):
        tipo = msg.get("tipo")
        if tipo == "auth":
            return self.autenticar(msg.get("metodo"), msg.get("valor"), msg.get("t"))
        if tipo == "pin_progreso":
            n = int(msg.get("n", 0))
            if n:
                self.mensaje("Ingrese su PIN:", "", "*" * n, "# = OK   * = borrar", duracion=config.PIN_TIMEOUT_S)
            else:
                self.pantalla_espera()
            return {"ok": True}
        if tipo == "abrir_remoto":                      # desde la app web
            detalle = self.abrir(msg.get("usuario", "admin"), "remoto", msg.get("t"))
            return {"ok": True, "detalle": detalle}
        if tipo == "lcd":                               # mensajes del registro de huella
            self.mensaje(*msg.get("lineas", [])[:4], duracion=msg.get("duracion", 3))
            return {"ok": True}
        if tipo == "registrar":                         # desde la app web
            return self.iniciar_registro(msg.get("usuario"), msg.get("metodo"))
        if tipo == "registro_estado":
            self.registro_activo()                      # marca "tiempo agotado" si ya venció
            r = self.registro
            if not r or r["id"] != msg.get("id"):
                return {"ok": False, "error": "registro desconocido"}
            return {"ok": True, "segundos": max(0, int(r["vence"] - time.time())),
                    **{k: r[k] for k in ("id", "usuario", "metodo", "estado", "mensaje", "valor")}}
        if tipo == "registro_resultado":                # de entradas, al terminar con la huella
            return self.resultado_huella(msg)
        if tipo == "baja":                              # desde la app web
            return self.baja(msg.get("usuario"), msg.get("admin"))
        if tipo == "alerta":                            # de entradas o de la web
            db.alerta(self.con, msg.get("origen", "sistema"), msg.get("mensaje", ""), msg.get("nivel", "aviso"))
            return {"ok": True}
        if tipo == "estado":
            with self.mutex:
                puerta = self.estado_puerta
            return {"ok": True, "puerta": puerta, "desbloqueada": self.desbloqueada,
                    "bloqueado": time.monotonic() < self.bloqueado_hasta}
        return {"ok": False, "error": f"tipo desconocido: {tipo}"}

    # ------------------------------------------------ hilo del reed switch
    def hilo_puerta(self):
        # Bitácora clase 11: el estado de la puerta llega por una cola de mensajes POSIX.
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
            db.log(self.con, "puerta", estado)
            log.info("puerta %s%s", estado, " (sin autorizacion)" if forzada else "")
            if forzada:
                db.alerta(self.con, "puerta", "abierta sin autorización", "critica")
                self.mensaje("ALERTA", "Puerta abierta sin", "autorizacion", duracion=5)
            else:
                self.lcd_linea_puerta()

    def vigilar_puerta(self):
        """Alerta una vez si la puerta se queda abierta más de PUERTA_ABIERTA_ALERTA_S.
        Espera con la misma Condition que el hilo del reed: no sondea."""
        limite = config.PUERTA_ABIERTA_ALERTA_S
        while True:
            with self.cambio_puerta:
                self.cambio_puerta.wait_for(lambda: self.estado_puerta == "abierta")
                cerro = self.cambio_puerta.wait_for(lambda: self.estado_puerta != "abierta", timeout=limite)
            if cerro:
                continue
            db.alerta(self.con, "puerta", f"lleva más de {limite} s abierta", "aviso")
            self.mensaje("AVISO", "La puerta sigue", "abierta", "Cierrela por favor", duracion=5)
            with self.cambio_puerta:                    # una sola alerta hasta que la cierren
                self.cambio_puerta.wait_for(lambda: self.estado_puerta != "abierta")

    def lcd_linea_puerta(self):
        if not hasattr(self.lcd, "linea"):
            return self.pantalla_espera()
        with self.mutex:
            puerta = self.estado_puerta
        self.lcd.linea(3, f"Puerta: {puerta}")

    # ------------------------------------------------ servidor
    def servir(self):
        threading.Thread(target=self.hilo_puerta, name="reed", daemon=True).start()
        threading.Thread(target=self.vigilar_puerta, name="vigia", daemon=True).start()
        if os.path.exists(config.SOCKET_PATH):
            os.unlink(config.SOCKET_PATH)
        # Bitácora clase 11: socket UNIX; las solicitudes se atienden una por una (FIFO).
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
            self.apagar_lcd()

    def apagar_lcd(self):
        """Al salir (systemctl stop o Ctrl+C) deja el LCD limpio y sin luz."""
        try:
            if hasattr(self.lcd, "apagar"):
                self.lcd.apagar()
            else:
                self.lcd.mostrar()                      # simulado: pantalla en blanco
        except OSError as e:
            log.warning("no se pudo limpiar el LCD: %s", e)


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
    Control(sim="--sim" in sys.argv).servir()


if __name__ == "__main__":
    main()