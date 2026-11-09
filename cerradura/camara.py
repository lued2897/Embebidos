"""Cámara Raspberry Pi -> app web.

odifica el video en MJPEG y lo escribe a su salida estándar; 
este módulo la lee por un pipe (bitácora clase 9) y separa 
los cuadros JPEG por sus marcadores.

- La cámara solo se enciende mientras alguien ve el video y se apaga 10 s
- Corre con nice +10.
- Si rpicam-vid se cae, se vuelve a lanzar a los 2 s.
"""
import logging
import os
import shlex
import shutil
import subprocess
import threading
import time

import config

log = logging.getLogger("camara")

SOI, EOI = b"\xff\xd8", b"\xff\xd9"
APAGAR_SIN_ESPECTADORES_S = 10
REINTENTO_S = 2
TAM_LECTURA = 64 * 1024


def comando():
    """Comando de captura. CERRADURA_CAMARA_CMD lo reemplaza (pruebas sin cámara)."""
    propio = os.environ.get("CERRADURA_CAMARA_CMD")
    if propio:
        return shlex.split(propio)
    binario = shutil.which("rpicam-vid") or shutil.which("libcamera-vid")
    if not binario:
        raise FileNotFoundError("no se encontró rpicam-vid ni libcamera-vid")
    return [binario, "-t", "0", "-n", "--codec", "mjpeg",
            "--width", str(config.CAMARA_ANCHO), "--height", str(config.CAMARA_ALTO),
            "--framerate", str(config.CAMARA_FPS), "-o", "-"]


def separar_cuadros(buffer):
    """Devuelve (cuadros completos, resto sin terminar) a partir de bytes MJPEG."""
    cuadros = []
    while True:
        inicio = buffer.find(SOI)
        if inicio < 0:
            return cuadros, b""
        fin = buffer.find(EOI, inicio + 2)
        if fin < 0:
            return cuadros, buffer[inicio:]
        cuadros.append(buffer[inicio:fin + 2])
        buffer = buffer[fin + 2:]


class Camara:
    def __init__(self):
        self.cond = threading.Condition()
        self.cuadro = None
        self.numero = 0
        self.espectadores = 0
        self.ultimo_espectador = 0.0                 # arranca apagada hasta que alguien la vea
        self.proc = None
        self.error = None
        self.cuadros_total = 0
        self.reinicios = 0
        threading.Thread(target=self._hilo, name="camara", daemon=True).start()

    # ------------------------------------------------ captura
    def _hilo(self):
        while True:
            if self.espectadores == 0 and time.monotonic() - self.ultimo_espectador > APAGAR_SIN_ESPECTADORES_S:
                time.sleep(0.2)
                continue
            try:
                self._capturar()
                continue                                 # se apagó por falta de espectadores
            except Exception as e:
                self.error = str(e)
                self.reinicios += 1
                log.error("camara: %s; reintento en %d s", e, REINTENTO_S)
            time.sleep(REINTENTO_S)

    def _capturar(self):
        cmd = comando()
        log.info("iniciando camara: %s", " ".join(cmd))
        self.proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            preexec_fn=lambda: os.nice(config.NICE_CAMARA))
        self.error = None
        resto = b""
        try:
            while True:
                datos = self.proc.stdout.read1(TAM_LECTURA)
                if not datos:
                    raise RuntimeError(f"rpicam-vid terminó (código {self.proc.poll()})")
                cuadros, resto = separar_cuadros(resto + datos)
                if cuadros:
                    with self.cond:
                        self.cuadro = cuadros[-1]
                        self.numero += 1
                        self.cuadros_total += len(cuadros)
                        self.cond.notify_all()
                if self.espectadores == 0 and time.monotonic() - self.ultimo_espectador > APAGAR_SIN_ESPECTADORES_S:
                    log.info("sin espectadores: apagando camara")
                    return
        finally:
            if self.proc.poll() is None:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    self.proc.kill()

    # ------------------------------------------------ consumo
    def flujo(self):
        """Generador multipart/x-mixed-replace para el navegador."""
        with self.cond:
            self.espectadores += 1
        visto = -1
        try:
            while True:
                with self.cond:
                    hay_nuevo = self.cond.wait_for(
                        lambda: self.cuadro is not None and self.numero != visto, timeout=5)
                    if not hay_nuevo:
                        continue                         # sin cuadros nuevos: sigue esperando
                    cuadro, visto = self.cuadro, self.numero
                yield (b"--cuadro\r\nContent-Type: image/jpeg\r\nContent-Length: "
                       + str(len(cuadro)).encode() + b"\r\n\r\n" + cuadro + b"\r\n")
        finally:
            with self.cond:
                self.espectadores -= 1
                self.ultimo_espectador = time.monotonic()

    def estado(self):
        return {"encendida": bool(self.proc and self.proc.poll() is None),
                "espectadores": self.espectadores, "cuadros": self.cuadros_total,
                "reinicios": self.reinicios, "error": self.error}
