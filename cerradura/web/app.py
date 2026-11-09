#!/usr/bin/env python3
"""Proceso WEB: app para el administrador en la red local.

- Inicio de sesión con contraseña (scrypt) y sesión que caduca tras 10 min sin actividad.
- Bloqueo de 60 s tras 5 intentos de login fallidos por dirección IP.
- Estado de la puerta (se consulta cada 500 ms), apertura remota, video en vivo,
  historial de eventos y administración de usuarios.
- Nunca toca el hardware: la apertura y el estado se piden al proceso de control
  por el mismo socket FIFO que usan las entradas (bitácora clases 9 y 11).

Uso:
    python3 web/app.py            # escucha en 0.0.0.0:8000
"""
import functools
import logging
import os
import secrets
import sqlite3
import sys
import threading
import time
from datetime import timedelta

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from flask import (Flask, Response, abort, flash, g, jsonify, redirect,  # noqa: E402
                   render_template, request, session, url_for)

import config  # noqa: E402
import db  # noqa: E402
import ipc  # noqa: E402
from camara import Camara  # noqa: E402

log = logging.getLogger("web")


def llave_sesiones():
    """Llave secreta para firmar las cookies de sesión; se crea una vez (0600)."""
    os.makedirs(config.DATA_DIR, exist_ok=True)
    if not os.path.exists(config.WEB_SECRET_PATH):
        tmp = config.WEB_SECRET_PATH + ".tmp"
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        os.write(fd, secrets.token_bytes(32))
        os.fsync(fd)
        os.close(fd)
        os.rename(tmp, config.WEB_SECRET_PATH)        # bitácora clase 15: temporal + fsync + rename
    with open(config.WEB_SECRET_PATH, "rb") as f:
        return f.read()


app = Flask(__name__)
app.config.update(
    SECRET_KEY=llave_sesiones(),
    PERMANENT_SESSION_LIFETIME=timedelta(minutes=config.WEB_SESION_MIN),
    SESSION_REFRESH_EACH_REQUEST=True,                 # cada petición renueva los 10 min
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Strict",
)
camara = Camara() if config.CAMARA_ACTIVA else None

_fallos_login = {}                                     # ip -> (fallos, bloqueado_hasta)
_fallos_lock = threading.Lock()


# ---------------------------------------------------------------- utilidades
def con():
    if "con" not in g:
        g.con = db.conectar()
    return g.con


@app.teardown_appcontext
def cerrar_con(_exc):
    c = g.pop("con", None)
    if c is not None:
        c.close()


def requiere_sesion(fn):
    @functools.wraps(fn)
    def envoltura(*a, **kw):
        if "usuario" not in session:
            if request.path.startswith("/api/") or request.path == "/video":
                abort(401)
            return redirect(url_for("login"))
        return fn(*a, **kw)
    return envoltura


def token_csrf():
    if "csrf" not in session:
        session["csrf"] = secrets.token_hex(16)
    return session["csrf"]


@app.context_processor
def plantilla_globales():
    return {"csrf": token_csrf, "estado_ms": config.WEB_ESTADO_MS}


@app.before_request
def revisar_csrf():
    if request.method == "POST":
        enviado = request.form.get("csrf") or request.headers.get("X-CSRF")
        if not enviado or enviado != session.get("csrf"):
            abort(400, "token CSRF inválido")


# ---------------------------------------------------------------- sesión
@app.route("/login", methods=["GET", "POST"])
def login():
    ip = request.remote_addr or "?"
    if request.method == "POST":
        with _fallos_lock:
            fallos, hasta = _fallos_login.get(ip, (0, 0.0))
        if time.monotonic() < hasta:
            flash(f"Demasiados intentos. Espere {int(hasta - time.monotonic()) + 1} s.")
            return render_template("login.html", sin_admins=False), 429
        usuario = request.form.get("usuario", "").strip()
        if db.verificar_admin(con(), usuario, request.form.get("password", "")):
            with _fallos_lock:
                _fallos_login.pop(ip, None)
            session.clear()
            session.permanent = True
            session["usuario"] = usuario
            db.log(con(), "web", "login correcto", usuario, f"ip {ip}")
            return redirect(url_for("panel"))
        fallos += 1
        if fallos >= config.WEB_MAX_INTENTOS:
            with _fallos_lock:
                _fallos_login[ip] = (0, time.monotonic() + config.WEB_BLOQUEO_S)
            db.log(con(), "web", "login bloqueado", usuario or None, f"ip {ip}, {fallos} intentos")
            flash(f"Demasiados intentos. Espere {config.WEB_BLOQUEO_S} s.")
        else:
            with _fallos_lock:
                _fallos_login[ip] = (fallos, 0.0)
            db.log(con(), "web", "login fallido", usuario or None, f"ip {ip}")
            flash("Usuario o contraseña incorrectos.")
    return render_template("login.html", sin_admins=not db.hay_admins(con()))


@app.post("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))


# ---------------------------------------------------------------- panel
@app.get("/")
@requiere_sesion
def panel():
    return render_template("panel.html", usuario=session["usuario"],
                           logs=db.ultimos_logs(con(), 10), camara=camara is not None)


@app.get("/api/estado")
@requiere_sesion
def api_estado():
    r = ipc.enviar({"tipo": "estado"}, timeout=1.0)
    r["camara"] = camara.estado() if camara else None
    return jsonify(r)


@app.post("/api/abrir")
@requiere_sesion
def api_abrir():
    r = ipc.enviar({"tipo": "abrir_remoto", "usuario": session["usuario"], "t": time.time()})
    return jsonify(r), (200 if r.get("ok") else 503)


@app.get("/video")
@requiere_sesion
def video():
    if not camara:
        abort(404)
    return Response(camara.flujo(), mimetype="multipart/x-mixed-replace; boundary=cuadro")


@app.get("/historial")
@requiere_sesion
def historial():
    n = min(int(request.args.get("n", 100)), 1000)
    return render_template("historial.html", usuario=session["usuario"], logs=db.ultimos_logs(con(), n), n=n)


# ---------------------------------------------------------------- usuarios
@app.get("/usuarios")
@requiere_sesion
def usuarios():
    return render_template("usuarios.html", usuario=session["usuario"], usuarios=db.listar_usuarios(con()))


@app.post("/usuarios")
@requiere_sesion
def usuarios_agregar():
    nombre = request.form.get("nombre", "").strip()
    pin = request.form.get("pin", "").strip()
    if not nombre:
        flash("Escriba un nombre.")
        return redirect(url_for("usuarios"))
    if pin and not (pin.isdigit() and config.PIN_MIN <= len(pin) <= config.PIN_MAX):
        flash(f"El PIN debe tener de {config.PIN_MIN} a {config.PIN_MAX} dígitos.")
        return redirect(url_for("usuarios"))
    try:
        db.crear_usuario(con(), nombre)
        if pin:
            db.asignar(con(), nombre, "pin_hash", db.hash_pin(pin))
        db.log(con(), "web", "usuario creado", session["usuario"], nombre)
        flash(f"Usuario {nombre} creado. Tarjeta y huella se registran con admin.py.")
    except sqlite3.IntegrityError:
        flash("Ese nombre o PIN ya está registrado.")
    return redirect(url_for("usuarios"))


@app.post("/usuarios/<nombre>/eliminar")
@requiere_sesion
def usuarios_eliminar(nombre):
    if db.eliminar_usuario(con(), nombre):
        db.log(con(), "web", "usuario eliminado", session["usuario"], nombre)
        flash(f"Usuario {nombre} eliminado.")
    return redirect(url_for("usuarios"))


def main():
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(message)s")
    with db.conectar() as c:
        if not db.hay_admins(c):
            log.warning("no hay administradores web: créelo con  admin.py web-admin <usuario>")
    app.run(host="0.0.0.0", port=config.WEB_PUERTO, threaded=True)


if __name__ == "__main__":
    main()