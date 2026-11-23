#!/usr/bin/env python3
"""Mediciones del reporte final (Avance 4) en la Raspberry Pi, con los servicios corriendo.

    cd /opt/cerradura
    sudo venv/bin/python medir_final.py recursos 60          # CPU y memoria en reposo
    sudo venv/bin/python medir_final.py recursos 60          # otra vez con el video abierto en la app
    sudo venv/bin/python medir_final.py puerta 20 grecia     # estado de la puerta en la app (< 1 s, 20 de 20)
    sudo venv/bin/python medir_final.py rechazo              # 30 intentos inválidos (30 de 30 rechazados)
    sudo venv/bin/python medir_final.py eventos 50           # 50 de 50 eventos con fecha, hora y acción
    sudo venv/bin/python medir_final.py persistencia guardar # antes de los 10 reinicios forzados
    sudo venv/bin/python medir_final.py persistencia verificar   # después de cada reinicio

    sudo venv/bin/python medir_final.py todo                 # todo lo anterior seguido, guiado paso a paso

Cada RESULTADO también se agrega a /opt/cerradura/resultados_medicion.txt.

Cada comando imprime una línea RESULTADO que va directo a la tabla del reporte.
"""
import ctypes
import getpass
import http.client
import shutil
import http.cookiejar
import json
import mmap
import os
import re
import sqlite3
import ssl
import subprocess
import sys
import threading
import time
import urllib.parse
import urllib.request

import config
import db

HZ = os.sysconf("SC_CLK_TCK")
SERVICIOS = ["cerradura-control", "cerradura-entradas", "cerradura-web"]
ESTADO = os.path.join(config.DATA_DIR, "persistencia.json")
RESULTADOS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "resultados_medicion.txt")
UNIDAD = "/etc/systemd/system/cerradura-medir-persistencia.service"
REINICIOS = 10


def resultado(texto):
    """Imprime la línea RESULTADO y la agrega al archivo de resultados."""
    print(texto, flush=True)
    with open(RESULTADOS, "a") as f:
        f.write(time.strftime("%Y-%m-%d %H:%M:%S ") + texto + "\n")


# ---------------------------------------------------------------- CPU y memoria
def pid_de(servicio):
    out = subprocess.run(["systemctl", "show", "-p", "MainPID", "--value", servicio], capture_output=True, text=True)
    pid = int(out.stdout.strip() or 0)
    return pid or None


def ticks(pid):
    campos = open(f"/proc/{pid}/stat").read().rsplit(")", 1)[1].split()
    return int(campos[11]) + int(campos[12])            # utime + stime


def rss_mb(pid):
    for linea in open(f"/proc/{pid}/status"):
        if linea.startswith("VmRSS:"):
            return int(linea.split()[1]) / 1024
    return 0.0


def cpu_total():
    v = [int(x) for x in open("/proc/stat").readline().split()[1:]]
    return sum(v), v[3] + v[4]                           # total, idle + iowait


def recursos(segundos=60):
    procesos = {s: pid_de(s) for s in SERVICIOS}
    out = subprocess.run(["pgrep", "-f", "rpicam-vid"], capture_output=True, text=True).stdout.split()
    if out:
        procesos["rpicam-vid (cámara)"] = int(out[0])
    procesos = {k: v for k, v in procesos.items() if v}
    t0, c0, i0 = time.time(), *cpu_total()
    inicio = {k: ticks(p) for k, p in procesos.items()}
    memoria = {k: [] for k in procesos}
    while time.time() - t0 < segundos:
        for k, p in procesos.items():
            memoria[k].append(rss_mb(p))
        time.sleep(1)
    dt, c1, i1 = time.time() - t0, *cpu_total()
    print(f"{'Proceso':<24} {'CPU %':>7} {'RSS MB':>8}")
    for k, p in procesos.items():
        cpu = 100 * (ticks(p) - inicio[k]) / HZ / dt       # % de UN núcleo
        print(f"{k:<24} {cpu:7.1f} {sum(memoria[k]) / len(memoria[k]):8.1f}")
    ocupado = 100 * (1 - (i1 - i0) / (c1 - c0))
    mem = {l.split(":")[0]: int(l.split()[1]) for l in open("/proc/meminfo")}
    usada = (mem["MemTotal"] - mem["MemAvailable"]) / 1024
    resultado(f"RESULTADO recursos: CPU total {ocupado:.1f} % (4 núcleos), memoria usada {usada:.0f} de "
          f"{mem['MemTotal'] / 1024:.0f} MB, {dt:.0f} s, cámara {'encendida' if 'rpicam-vid (cámara)' in procesos else 'apagada'}")


# ---------------------------------------------------------------- estado de la puerta en la app
def sesion_web(usuario):
    """Inicia sesión en la app y regresa una función que consulta /api/estado.

    Usa una sola conexión que se reutiliza (HTTP/1.1), para no repetir el saludo TLS en cada consulta,
    y cada consulta tiene un tiempo límite: si la app no contesta, se reporta en vez de quedarse esperando.
    """
    clave = getpass.getpass(f"Contraseña web de {usuario}: ")
    https = os.path.exists(getattr(config, "WEB_CERT", ""))      # con HTTPS usa el certificado de la Pi
    ctx = None
    if https:
        ctx = ssl.create_default_context(cafile=config.WEB_CERT)
        ctx.check_hostname = False                                 # 127.0.0.1 puede no estar en el certificado
    jar = http.cookiejar.CookieJar()
    op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar), urllib.request.HTTPSHandler(context=ctx))
    base = f"{'https' if https else 'http'}://127.0.0.1:{config.WEB_PUERTO}"
    html = op.open(base + "/login", timeout=10).read().decode()
    tok = re.search(r'name="csrf" value="([0-9a-f]+)"', html).group(1)
    datos = urllib.parse.urlencode({"csrf": tok, "usuario": usuario, "password": clave}).encode()
    op.open(base + "/login", datos, timeout=10)
    cookie = "; ".join(f"{c.name}={c.value}" for c in jar)
    conn = [None]

    def estado():
        for intento in range(2):
            try:
                if conn[0] is None:
                    conn[0] = (http.client.HTTPSConnection("127.0.0.1", config.WEB_PUERTO, context=ctx, timeout=3)
                               if https else http.client.HTTPConnection("127.0.0.1", config.WEB_PUERTO, timeout=3))
                conn[0].request("GET", "/api/estado", headers={"Cookie": cookie})
                r = conn[0].getresponse()
                cuerpo = r.read()
                if r.status != 200:
                    raise RuntimeError(f"la app respondió {r.status} (¿sesión caducada o contraseña incorrecta?)")
                return json.loads(cuerpo)
            except (OSError, http.client.HTTPException) as e:
                if conn[0]:
                    conn[0].close()
                conn[0] = None
                if intento:
                    return {"puerta": None, "error": str(e) or type(e).__name__}
    primero = estado()
    if primero.get("puerta") is None:
        raise SystemExit(f"La app no contesta /api/estado: {primero.get('error')}")
    return estado


def pinctrl_17():
    """Nivel de GPIO17 según 'pinctrl get' (la herramienta oficial): 1, 0 o None si no se pudo."""
    if not shutil.which("pinctrl"):
        return None
    out = subprocess.run(["pinctrl", "get", str(config.REED_GPIO)], capture_output=True, text=True).stdout
    return 1 if "| hi" in out else 0 if "| lo" in out else None


def nivel_gpio17():
    """Nivel del reed sin ocupar el pin (el proceso de entradas lo tiene).

    Lee el registro GPLEV0 por /dev/gpiomem con un acceso de 32 bits (los registros del
    BCM2837 no admiten lecturas de un byte). Si no se puede, usa pinctrl.
    """
    try:
        fd = os.open("/dev/gpiomem", os.O_RDWR | os.O_SYNC)
        m = mmap.mmap(fd, 4096, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
        reg = ctypes.c_uint32.from_buffer(m, 0x34)                # GPLEV0, solo se lee
        leer = lambda: (reg.value >> config.REED_GPIO) & 1
        leer.nombre = "gpiomem"
        return leer
    except (OSError, TypeError, ValueError) as e:
        print(f"(no se pudo leer /dev/gpiomem: {e}; se usa pinctrl)")
    if pinctrl_17() is None:
        raise SystemExit("No hay forma de leer GPIO17 (ni /dev/gpiomem ni pinctrl)")
    pinctrl_17.nombre, pinctrl_17.lento = "pinctrl", True
    return pinctrl_17


def puerta(n=20, usuario="admin", estado=None, leer=None, limite_s=180):
    estado = estado or sesion_web(usuario)
    leer = leer or nivel_gpio17()
    nombre = lambda nv: "cerrada" if nv == 0 else "abierta"   # imán cerca = 0 (pull-up)
    nivel = leer()
    app = estado()
    print(f"Ahora: imán dice '{nombre(nivel)}', la app dice '{app.get('puerta')}'.")
    print(f"Abra y cierre la puerta {n // 2} veces, despacio (espere 2 s entre cada cambio). Ctrl+C para terminar antes.")
    medidas, ultimo_aviso, t_inicio = [], time.time(), time.time()
    try:
        while len(medidas) < n:
            if time.time() - t_inicio > limite_s:
                print(f"Pasaron {limite_s} s; se termina con {len(medidas)} medidas.")
                break
            actual = leer()
            if actual != nivel:                                   # cambió el imán: empieza a contar
                t0 = time.perf_counter()
                time.sleep(0.06)                                  # rebote del reed (entradas usa 50 ms)
                if leer() != actual:
                    continue
                nivel = actual
                esperado, visto = nombre(nivel), None
                while time.perf_counter() - t0 < 3:
                    visto = estado().get("puerta")
                    if visto == esperado:
                        break
                    time.sleep(0.05)
                ms = (time.perf_counter() - t0) * 1000
                marca = "" if visto == esperado else f"  <-- la app dijo '{visto}'"
                medidas.append(ms)
                print(f"{len(medidas):2d}. {esperado:<8} {ms:7.1f} ms{marca}", flush=True)
            elif time.time() - ultimo_aviso > 2:                  # cada 2 s: señal de vida y comprobación
                ultimo_aviso = time.time()
                pc = pinctrl_17() if getattr(leer, "nombre", "") != "pinctrl" else None
                if pc is not None and pc != leer():               # la lectura directa no coincide: usar pinctrl
                    print("   (la lectura de /dev/gpiomem no coincide con pinctrl; se usa pinctrl)", flush=True)
                    pinctrl_17.nombre, pinctrl_17.lento = "pinctrl", True
                    leer = pinctrl_17
                    nivel = leer()                                # parte del nivel real; cuenta desde el próximo cambio
                    continue
                print(f"   ...esperando un cambio (imán: {nombre(nivel)}, app: {estado().get('puerta')}, "
                      f"lectura: {getattr(leer, 'nombre', '?')})", flush=True)
            time.sleep(0.02 if getattr(leer, 'lento', False) else 0.005)
    except KeyboardInterrupt:
        print("\nMedición detenida.")
    if not medidas:
        resultado("RESULTADO puerta: sin medidas (el imán no cambió de nivel; revise el reed en GPIO17)")
        return
    m = len(medidas)
    peor = max(medidas) + config.WEB_ESTADO_MS       # la página consulta cada 500 ms
    ok = sum(1 for x in medidas if x + config.WEB_ESTADO_MS < 1000)
    resultado(f"RESULTADO puerta: mediana {sorted(medidas)[m // 2]:.0f} ms, máxima {max(medidas):.0f} ms hasta la API; "
              f"peor caso en la página {peor:.0f} ms (+{config.WEB_ESTADO_MS} ms de consulta); {ok} de {m} en < 1 s")


# ---------------------------------------------------------------- rechazo, eventos, persistencia
def rechazo():
    con = db.conectar()
    desde = con.execute("SELECT COALESCE(MAX(id), 0) FROM logs").fetchone()[0]
    input("Haga 30 intentos inválidos (10 tarjetas, 10 PIN y 10 huellas no registradas) y pulse Enter... ")
    filas = con.execute("SELECT metodo, resultado FROM logs WHERE id > ? AND metodo IN ('rfid','pin','huella')",
                        (desde,)).fetchall()
    negados = sum(1 for f in filas if f["resultado"] == "denegado")
    abiertos = sum(1 for f in filas if f["resultado"] == "concedido")
    resultado(f"RESULTADO rechazo: {negados} de {len(filas)} rechazados, {abiertos} aperturas indebidas")


def eventos(n=50):
    con = db.conectar()
    filas = con.execute("SELECT * FROM logs ORDER BY id DESC LIMIT ?", (n,)).fetchall()
    completos = sum(1 for f in filas if re.fullmatch(r"\d{4}-\d\d-\d\d \d\d:\d\d:\d\d", f["fecha"] or "")
                    and f["metodo"] and f["resultado"])
    resultado(f"RESULTADO eventos: {completos} de {len(filas)} con fecha, hora y acción")


def persistencia(accion):
    con = db.conectar()
    if accion == "guardar":
        n, ultimo = con.execute("SELECT COUNT(*), MAX(id) FROM logs").fetchone()
        json.dump({"n": n, "ultimo": ultimo, "usuarios": con.execute("SELECT COUNT(*) FROM usuarios").fetchone()[0],
                   "reinicios": 0}, open(ESTADO, "w"))
        instalar_verificacion()
        print(f"Guardado: {n} eventos (último id {ultimo}). Corte la luz {REINICIOS} veces (espere a que arranque "
              f"cada vez); la verificación corre sola al encender y se agrega a {RESULTADOS}.")
        return
    e = json.load(open(ESTADO))
    e["reinicios"] += 1
    json.dump(e, open(ESTADO, "w"))
    presentes = con.execute("SELECT COUNT(*) FROM logs WHERE id <= ?", (e["ultimo"],)).fetchone()[0]
    usuarios = con.execute("SELECT COUNT(*) FROM usuarios").fetchone()[0]
    integridad = con.execute("PRAGMA integrity_check").fetchone()[0]
    perdidos = e["n"] - presentes
    resultado(f"RESULTADO persistencia (reinicio {e['reinicios']} de {REINICIOS}): {perdidos} registros perdidos, "
              f"usuarios {usuarios}/{e['usuarios']}, integrity_check {integridad}")
    if e["reinicios"] >= REINICIOS and os.path.exists(UNIDAD):
        subprocess.run(["systemctl", "disable", "cerradura-medir-persistencia"], capture_output=True)
        os.remove(UNIDAD)
        subprocess.run(["systemctl", "daemon-reload"], capture_output=True)
        resultado("RESULTADO persistencia: terminaron los 10 reinicios; verificación automática desactivada")


def instalar_verificacion():
    """Servicio que corre 'persistencia verificar' en cada arranque hasta completar los reinicios."""
    aqui = os.path.dirname(os.path.abspath(__file__))
    with open(UNIDAD, "w") as f:
        f.write(f"""[Unit]
Description=Cerradura - verificar persistencia tras un corte (medicion del reporte)
After=cerradura-control.service

[Service]
Type=oneshot
WorkingDirectory={aqui}
ExecStartPre=/bin/sleep 10
ExecStart={sys.executable} {os.path.abspath(__file__)} persistencia verificar

[Install]
WantedBy=multi-user.target
""")
    subprocess.run(["systemctl", "daemon-reload"], check=True)
    subprocess.run(["systemctl", "enable", "cerradura-medir-persistencia"], check=True, capture_output=True)


def pausa(texto):
    input(f"\n>>> {texto} Pulse Enter para seguir... ")


def todo():
    print(f"Mediciones del reporte final. Los resultados se guardan en {RESULTADOS}\n")
    usuario = input("Usuario administrador de la app web (para medir el estado de la puerta): ").strip() or "admin"
    print("\n[1/6] CPU y memoria en reposo (60 s)")
    pausa("Cierre el video en el navegador y no toque la cerradura.")
    recursos(60)
    print("\n[2/6] CPU y memoria con el video abierto (60 s)")
    pausa("Abra el panel de la app con el video en vivo y déjelo abierto.")
    recursos(60)
    print("\n[3/6] Estado de la puerta en la app")
    try:
        puerta(20, usuario)
    except SystemExit as e:
        print(e, "-> se salta este paso")
    print("\n[4/6] Rechazo de credenciales inválidas")
    rechazo()
    print("\n[5/6] Registro de eventos")
    eventos(50)
    print("\n[6/6] Persistencia ante cortes de luz")
    persistencia("guardar")
    print("\nResumen:")
    for l in open(RESULTADOS):
        if "RESULTADO" in l:
            print("  " + l.rstrip())


def main():
    a = sys.argv[1:] or ["ayuda"]
    if a[0] == "recursos":
        recursos(int(a[1]) if len(a) > 1 else 60)
    elif a[0] == "puerta":
        puerta(int(a[1]) if len(a) > 1 else 20, a[2] if len(a) > 2 else "admin")
    elif a[0] == "rechazo":
        rechazo()
    elif a[0] == "eventos":
        eventos(int(a[1]) if len(a) > 1 else 50)
    elif a[0] == "todo":
        todo()
    elif a[0] == "persistencia":
        persistencia(a[1] if len(a) > 1 else "verificar")
    else:
        print(__doc__)


if __name__ == "__main__":
    main()