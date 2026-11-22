#!/usr/bin/env python3
"""Respaldo de la información: la base de datos y la llave de los PIN.

Sin UPS, un corte de luz a mitad de una escritura puede dañar la microSD (riesgo
del Avance 1). La base ya usa WAL + synchronous=FULL; además, una vez al día
(cerradura-respaldo.timer) se guarda una copia completa en:
  - la propia tarjeta (DATA_DIR/respaldos): protege de una base dañada;
  - una memoria USB montada en /mnt/respaldo, si está conectada: protege si la
    tarjeta misma falla.

Cada respaldo es una carpeta respaldo_AAAAMMDD_HHMMSS con:
  cerradura.db  copia consistente hecha con la API de respaldo de SQLite
                (no hace falta detener los servicios)
  pepper.key    llave de los PIN: sin ella los PIN guardados no sirven
  info.json     fecha, conteos y SHA-256 de la copia, para verificarla al restaurar

Uso:
    python3 respaldo.py            # hace un respaldo (lo corre el timer de systemd)
    python3 admin.py respaldos     # lista los respaldos
    python3 admin.py restaurar <carpeta>   # con los servicios detenidos
"""
import hashlib
import json
import os
import shutil
import sqlite3
import sys
from datetime import datetime

import config
import db

PREFIJO = "respaldo_"


def destinos():
    """La carpeta local siempre; la de la memoria USB solo si está montada."""
    lista = [config.RESPALDO_LOCAL]
    if os.path.ismount(config.RESPALDO_USB):
        lista.append(os.path.join(config.RESPALDO_USB, "cerradura"))
    return lista


def sha256(ruta):
    h = hashlib.sha256()
    with open(ruta, "rb") as f:
        for bloque in iter(lambda: f.read(65536), b""):
            h.update(bloque)
    return h.hexdigest()


def _fsync(ruta):
    """fsync de un archivo o de una carpeta (para que el rename también quede en disco)."""
    try:
        fd = os.open(ruta, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    except OSError:
        pass                                        # vfat no deja hacer fsync a una carpeta


def _copiar_base(destino):
    origen = db.conectar()
    copia = sqlite3.connect(destino)
    try:
        origen.backup(copia)                        # copia consistente aunque el control esté escribiendo
        copia.execute("PRAGMA journal_mode=DELETE") # un solo archivo, sin -wal
        estado = copia.execute("PRAGMA integrity_check").fetchone()[0]
        if estado != "ok":
            raise RuntimeError(f"la copia no pasó integrity_check: {estado}")
        return {t: copia.execute(f"SELECT COUNT(*) FROM {t}").fetchone()[0]
                for t in ("usuarios", "logs", "alertas")}
    finally:
        copia.close()
        origen.close()


def listar_en(carpeta):
    if not os.path.isdir(carpeta):
        return []
    return sorted(n for n in os.listdir(carpeta) if n.startswith(PREFIJO) and not n.endswith(".tmp"))


def rotar(carpeta):
    for nombre in os.listdir(carpeta):              # restos de un respaldo cortado a la mitad
        if nombre.startswith(PREFIJO) and nombre.endswith(".tmp"):
            shutil.rmtree(os.path.join(carpeta, nombre), ignore_errors=True)
    for nombre in listar_en(carpeta)[:-config.RESPALDO_CONSERVAR]:
        shutil.rmtree(os.path.join(carpeta, nombre), ignore_errors=True)


def hacer_en(carpeta, fecha):
    os.makedirs(carpeta, mode=0o700, exist_ok=True)
    final, n = os.path.join(carpeta, PREFIJO + fecha), 1
    while os.path.exists(final):                    # dos respaldos en el mismo segundo
        n += 1
        final = os.path.join(carpeta, f"{PREFIJO}{fecha}_{n}")
    tmp = final + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp, mode=0o700)
    base = os.path.join(tmp, "cerradura.db")
    conteo = _copiar_base(base)
    if os.path.exists(config.PEPPER_PATH):
        shutil.copyfile(config.PEPPER_PATH, os.path.join(tmp, "pepper.key"))
        os.chmod(os.path.join(tmp, "pepper.key"), 0o600)
    info = {"fecha": datetime.now().isoformat(sep=" ", timespec="seconds"), "conteo": conteo,
            "bytes": os.path.getsize(base), "sha256": sha256(base)}
    with open(os.path.join(tmp, "info.json"), "w") as f:
        json.dump(info, f, indent=1)
    for nombre in os.listdir(tmp):
        _fsync(os.path.join(tmp, nombre))
    _fsync(tmp)
    os.rename(tmp, final)                           # bitácora clase 15: aparece completo o no aparece
    _fsync(carpeta)
    rotar(carpeta)
    return final, info


def respaldar():
    """Respalda en todos los destinos disponibles. Devuelve [(carpeta, info)];
    si ninguno salió, deja una alerta crítica y lanza la excepción."""
    fecha = datetime.now().strftime("%Y%m%d_%H%M%S")
    hechos, errores = [], []
    for carpeta in destinos():
        try:
            hechos.append(hacer_en(carpeta, fecha))
        except Exception as e:
            errores.append(f"{carpeta}: {e}")
    con = db.conectar()
    try:
        if hechos:
            donde = " y ".join("USB" if r.startswith(config.RESPALDO_USB) else "tarjeta" for r, _ in hechos)
            db.log(con, "respaldo", "ok", detalle=f"{donde}, {hechos[0][1]['conteo']['logs']} eventos")
        if errores:
            db.alerta(con, "respaldo", "falló el respaldo en " + "; ".join(errores),
                      "aviso" if hechos else "critica")
    finally:
        con.close()
    if not hechos:
        raise RuntimeError("; ".join(errores))
    return hechos


def listar():
    """Todos los respaldos, del más nuevo al más viejo."""
    salida = []
    for carpeta in destinos():
        for nombre in listar_en(carpeta):
            ruta = os.path.join(carpeta, nombre)
            try:
                with open(os.path.join(ruta, "info.json")) as f:
                    info = json.load(f)
            except (OSError, ValueError):
                info = None
            salida.append({"ruta": ruta, "nombre": nombre, "info": info,
                           "destino": "USB" if ruta.startswith(config.RESPALDO_USB) else "tarjeta"})
    return sorted(salida, key=lambda r: r["nombre"], reverse=True)


def verificar(ruta):
    """Revisa que la copia sea la misma que se guardó (SHA-256) y que SQLite la acepte."""
    with open(os.path.join(ruta, "info.json")) as f:
        info = json.load(f)
    base = os.path.join(ruta, "cerradura.db")
    if sha256(base) != info["sha256"]:
        raise RuntimeError("la copia no coincide con su SHA-256: está dañada")
    con = sqlite3.connect(f"file:{base}?mode=ro", uri=True)
    try:
        if con.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise RuntimeError("la copia no pasa integrity_check")
    finally:
        con.close()
    return info


def restaurar(ruta, forzar=False):
    """Pone un respaldo como base actual. Antes guarda la base actual en
    respaldos/antes_de_restaurar_<fecha> por si hay que regresar."""
    if os.path.exists(config.SOCKET_PATH) and not forzar:
        raise RuntimeError("primero detenga los servicios: "
                           "sudo systemctl stop cerradura-web cerradura-entradas cerradura-control")
    info = verificar(ruta)
    previo = None
    if os.path.exists(config.DB_PATH):
        previo = os.path.join(config.RESPALDO_LOCAL, "antes_de_restaurar_" + datetime.now().strftime("%Y%m%d_%H%M%S"))
        os.makedirs(previo, mode=0o700)
        try:
            _copiar_base(os.path.join(previo, "cerradura.db"))
        except Exception:                           # la base actual está dañada: se copia tal cual
            for ext in ("", "-wal", "-shm"):
                if os.path.exists(config.DB_PATH + ext):
                    shutil.copyfile(config.DB_PATH + ext, os.path.join(previo, "cerradura.db" + ext))
        if os.path.exists(config.PEPPER_PATH):
            shutil.copyfile(config.PEPPER_PATH, os.path.join(previo, "pepper.key"))
    for origen, destino in (("cerradura.db", config.DB_PATH), ("pepper.key", config.PEPPER_PATH)):
        if not os.path.exists(os.path.join(ruta, origen)):
            continue
        tmp = destino + ".restaurando"
        shutil.copyfile(os.path.join(ruta, origen), tmp)
        os.chmod(tmp, 0o600)
        _fsync(tmp)
        os.replace(tmp, destino)                    # temporal + fsync + rename
    for ext in ("-wal", "-shm"):                    # eran de la base anterior
        if os.path.exists(config.DB_PATH + ext):
            os.remove(config.DB_PATH + ext)
    _fsync(config.DATA_DIR)
    return info, previo


def main():
    try:
        hechos = respaldar()
    except Exception as e:
        sys.exit(f"ERROR: {e}")
    for ruta, info in hechos:
        print(f"{ruta}  ({info['conteo']['usuarios']} usuarios, {info['conteo']['logs']} eventos, {info['bytes']} bytes)")
    if not os.path.ismount(config.RESPALDO_USB):
        print(f"Aviso: no hay memoria USB en {config.RESPALDO_USB}; solo se respaldó en la tarjeta")


if __name__ == "__main__":
    main()
