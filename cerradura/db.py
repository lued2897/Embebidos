"""Base de datos local (SQLite en la microSD): usuarios, logs y alertas.

- PINs guardados como HMAC-SHA256(pepper, pin): nunca en texto plano.
- journal_mode=WAL + synchronous=FULL: cada escritura queda en disco (fsync)
  antes de confirmarse; un corte a mitad de escritura no corrompe la base.
"""
import hashlib
import hmac
import os
import sqlite3
import threading
from datetime import datetime

import config

_lock = threading.Lock()
_pepper_cache = None


def _pepper() -> bytes:
    global _pepper_cache
    if _pepper_cache:
        return _pepper_cache
    os.makedirs(config.DATA_DIR, exist_ok=True)
    if not os.path.exists(config.PEPPER_PATH):
        tmp = config.PEPPER_PATH + ".tmp"
        fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        os.write(fd, os.urandom(32))
        os.fsync(fd)
        os.close(fd)
        os.rename(tmp, config.PEPPER_PATH)
    with open(config.PEPPER_PATH, "rb") as f:
        _pepper_cache = f.read()
    return _pepper_cache


def hash_pin(pin: str) -> str:
    return hmac.new(_pepper(), pin.encode(), hashlib.sha256).hexdigest()


def conectar() -> sqlite3.Connection:
    os.makedirs(config.DATA_DIR, exist_ok=True)
    con = sqlite3.connect(config.DB_PATH, check_same_thread=False)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("PRAGMA synchronous=FULL")
    # Control, entradas y web abren la misma base: si otro proceso está escribiendo,
    # se espera hasta 3 s en lugar de fallar con "database is locked".
    con.execute("PRAGMA busy_timeout=3000")
    con.executescript(
        """
        CREATE TABLE IF NOT EXISTS usuarios (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            nombre    TEXT NOT NULL UNIQUE,
            rfid_uid  TEXT UNIQUE,
            pin_hash  TEXT UNIQUE,
            huella_id INTEGER UNIQUE,
            activo    INTEGER NOT NULL DEFAULT 1,
            creado    TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS admins (
            usuario   TEXT PRIMARY KEY,
            pass_hash TEXT NOT NULL,
            creado    TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS logs (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            fecha     TEXT NOT NULL,
            metodo    TEXT NOT NULL,
            resultado TEXT NOT NULL,
            usuario   TEXT,
            detalle   TEXT
        );
        CREATE TABLE IF NOT EXISTS alertas (
            id      INTEGER PRIMARY KEY AUTOINCREMENT,
            fecha   TEXT NOT NULL,
            origen  TEXT NOT NULL,
            mensaje TEXT NOT NULL,
            nivel   TEXT NOT NULL,
            vista   INTEGER NOT NULL DEFAULT 0
        );
        """
    )
    con.commit()
    return con


# ---------------- Usuarios ----------------
def crear_usuario(con, nombre):
    with _lock:
        cur = con.execute(
            "INSERT INTO usuarios (nombre, creado) VALUES (?, ?)",
            (nombre, datetime.now().isoformat(timespec="seconds")),
        )
        con.commit()
        return cur.lastrowid


def asignar(con, nombre, campo, valor):
    if campo not in ("rfid_uid", "pin_hash", "huella_id"):
        raise ValueError(campo)
    with _lock:
        cur = con.execute(f"UPDATE usuarios SET {campo}=? WHERE nombre=?", (valor, nombre))
        con.commit()
        return cur.rowcount == 1


def eliminar_usuario(con, nombre):
    with _lock:
        cur = con.execute("DELETE FROM usuarios WHERE nombre=?", (nombre,))
        con.commit()
        return cur.rowcount == 1


def usuario(con, nombre):
    return con.execute("SELECT * FROM usuarios WHERE nombre=?", (nombre,)).fetchone()


def listar_usuarios(con):
    return con.execute(
        "SELECT id, nombre, rfid_uid, pin_hash IS NOT NULL AS tiene_pin, huella_id, activo "
        "FROM usuarios ORDER BY id"
    ).fetchall()


def buscar_por(con, metodo, valor):
    """Devuelve el nombre del usuario activo que coincide o None."""
    with _lock:
        return _buscar(con, metodo, valor)


def _buscar(con, metodo, valor):
    if metodo == "rfid":
        fila = con.execute("SELECT nombre FROM usuarios WHERE rfid_uid=? AND activo=1", (valor,)).fetchone()
    elif metodo == "pin":
        fila = con.execute("SELECT nombre FROM usuarios WHERE pin_hash=? AND activo=1", (hash_pin(valor),)).fetchone()
    elif metodo == "huella":
        try:
            valor = int(valor)
        except (TypeError, ValueError):
            return None
        fila = con.execute("SELECT nombre FROM usuarios WHERE huella_id=? AND activo=1", (valor,)).fetchone()
    else:
        return None
    return fila["nombre"] if fila else None


# ---------------- Logs ----------------
def log(con, metodo, resultado, usuario=None, detalle=None):
    with _lock:
        con.execute(
            "INSERT INTO logs (fecha, metodo, resultado, usuario, detalle) VALUES (?, ?, ?, ?, ?)",
            (datetime.now().isoformat(sep=" ", timespec="seconds"), metodo, resultado, usuario, detalle),
        )
        con.commit()


def ultimos_logs(con, n=30):
    return con.execute("SELECT * FROM logs ORDER BY id DESC LIMIT ?", (n,)).fetchall()


def latencias(con, n=200):
    """Latencias (ms) credencial -> servo de las últimas n aperturas medidas."""
    filas = con.execute(
        "SELECT metodo, detalle FROM logs WHERE resultado='concedido' AND detalle LIKE 'latencia %' "
        "ORDER BY id DESC LIMIT ?", (n,)).fetchall()
    return [(f["metodo"], float(f["detalle"].split()[1])) for f in filas]


# ---------------- Alertas ----------------
# critica: requiere atención (puerta forzada, bloqueo, sensor caído)
# aviso:   algo raro pero no urgente
# info:    se resolvió algo (un sensor volvió); se guarda ya como vista
NIVELES = ("critica", "aviso", "info")


def alerta(con, origen, mensaje, nivel="aviso"):
    """Guarda una alerta para la app web y la deja también en el historial."""
    nivel = nivel if nivel in NIVELES else "aviso"
    fecha = datetime.now().isoformat(sep=" ", timespec="seconds")
    with _lock:
        con.execute("INSERT INTO alertas (fecha, origen, mensaje, nivel, vista) VALUES (?, ?, ?, ?, ?)",
                    (fecha, origen, mensaje, nivel, 1 if nivel == "info" else 0))
        con.execute("INSERT INTO logs (fecha, metodo, resultado, detalle) VALUES (?, ?, 'alerta', ?)",
                    (fecha, origen, mensaje))
        con.commit()


def ultimas_alertas(con, n=50, solo_pendientes=False):
    filtro = "WHERE vista=0 " if solo_pendientes else ""
    return con.execute(f"SELECT * FROM alertas {filtro}ORDER BY id DESC LIMIT ?", (n,)).fetchall()


def alertas_pendientes(con):
    return con.execute("SELECT COUNT(*) FROM alertas WHERE vista=0").fetchone()[0]


def marcar_alertas_vistas(con, hasta_id=None):
    """Marca como vistas las alertas pendientes (hasta hasta_id, si se da)."""
    with _lock:
        if hasta_id is None:
            cur = con.execute("UPDATE alertas SET vista=1 WHERE vista=0")
        else:
            cur = con.execute("UPDATE alertas SET vista=1 WHERE vista=0 AND id<=?", (hasta_id,))
        con.commit()
        return cur.rowcount


# ---------------- Admins web ----------------
def _scrypt(password, sal):
    return hashlib.scrypt(password.encode(), salt=sal, n=2**14, r=8, p=1, dklen=32)


def crear_admin(con, usuario, password):
    sal = os.urandom(16)
    valor = f"scrypt${sal.hex()}${_scrypt(password, sal).hex()}"
    with _lock:
        con.execute("INSERT OR REPLACE INTO admins (usuario, pass_hash, creado) VALUES (?, ?, ?)",
                    (usuario, valor, datetime.now().isoformat(timespec="seconds")))
        con.commit()


def verificar_admin(con, usuario, password):
    fila = con.execute("SELECT pass_hash FROM admins WHERE usuario=?", (usuario,)).fetchone()
    if not fila:
        _scrypt(password, b"0" * 16)                 # mismo tiempo de respuesta exista o no el usuario
        return False
    _, sal, esperado = fila["pass_hash"].split("$")
    return hmac.compare_digest(_scrypt(password, bytes.fromhex(sal)).hex(), esperado)


def hay_admins(con):
    return con.execute("SELECT COUNT(*) FROM admins").fetchone()[0] > 0
