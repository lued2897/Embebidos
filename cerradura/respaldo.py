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
