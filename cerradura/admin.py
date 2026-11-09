#!/usr/bin/env python3
"""Herramienta de administración por consola.

    python3 admin.py agregar  <nombre>
    python3 admin.py pin      <nombre>            # pide el PIN (4-6 dígitos), se guarda con hash
    python3 admin.py tarjeta  <nombre> [UID]      # sin UID: lo lee del RC522
    python3 admin.py huella   <nombre>            # registra la huella en el AS608
    python3 admin.py eliminar <nombre>
    python3 admin.py listar
    python3 admin.py logs     [n]
    python3 admin.py abrir                        # apertura remota (prueba del servo)
    python3 admin.py estado
    python3 admin.py web-admin <usuario>          # crea o cambia un administrador de la app web
    python3 admin.py medir    [n]                 # latencia credencial -> servo de las últimas n aperturas

Para 'tarjeta' (sin UID) y 'huella' deténer antes el proceso de entradas: 
    sudo systemctl stop cerradura-entradas
"""
import getpass
import sqlite3
import sys
import time

import config
import db
import ipc


def leer_tarjeta():
    from hw.rc522 import RC522
    lector = RC522(config.RFID_SPI_BUS, config.RFID_SPI_DEV)
    print("Acerque la tarjeta al lector...")
    while True:
        uid = lector.leer_uid()
        if uid:
            lector.close()
            return uid
        time.sleep(0.1)


def sensor_huella():
    from pyfingerprint.pyfingerprint import PyFingerprint
    f = PyFingerprint(config.HUELLA_PUERTO, config.HUELLA_BAUD, 0xFFFFFFFF, 0x00000000)
    if not f.verifyPassword():
        sys.exit("Contraseña del sensor incorrecta")
    return f


def registrar_huella():
    f = sensor_huella()
    print("Coloque el dedo en el sensor...")
    while not f.readImage():
        time.sleep(0.1)
    f.convertImage(0x01)
    pos, _ = f.searchTemplate()
    if pos >= 0:
        sys.exit(f"Esa huella ya existe en la posición {pos}")
    print("Retire el dedo")
    time.sleep(1.5)
    while f.readImage():
        time.sleep(0.1)
    print("Coloque el MISMO dedo otra vez...")
    while not f.readImage():
        time.sleep(0.1)
    f.convertImage(0x02)
    if f.compareCharacteristics() == 0:
        sys.exit("Las dos lecturas no coinciden, intente de nuevo")
    f.createTemplate()
    return f.storeTemplate()


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]
    con = db.conectar()

    try:
        if cmd == "agregar":
            db.crear_usuario(con, args[0])
            print(f"Usuario '{args[0]}' creado")

        elif cmd == "pin":
            pin = getpass.getpass("PIN (4-6 dígitos): ")
            if not (pin.isdigit() and config.PIN_MIN <= len(pin) <= config.PIN_MAX):
                sys.exit("PIN inválido")
            if getpass.getpass("Repita el PIN: ") != pin:
                sys.exit("No coinciden")
            ok = db.asignar(con, args[0], "pin_hash", db.hash_pin(pin))
            print("PIN guardado" if ok else "Usuario no encontrado")

        elif cmd == "tarjeta":
            uid = args[1].upper() if len(args) > 1 else leer_tarjeta()
            ok = db.asignar(con, args[0], "rfid_uid", uid)
            print(f"Tarjeta {uid} asignada" if ok else "Usuario no encontrado")

        elif cmd == "huella":
            pos = registrar_huella()
            ok = db.asignar(con, args[0], "huella_id", pos)
            print(f"Huella guardada en la posición {pos}" if ok else "Usuario no encontrado")

        elif cmd == "eliminar":
            fila = con.execute("SELECT huella_id FROM usuarios WHERE nombre=?", (args[0],)).fetchone()
            if fila and fila["huella_id"] is not None:
                try:
                    sensor_huella().deleteTemplate(fila["huella_id"])
                except Exception as e:
                    print(f"Aviso: no se borró la plantilla del sensor ({e})")
            print("Eliminado" if db.eliminar_usuario(con, args[0]) else "Usuario no encontrado")

        elif cmd == "listar":
            print(f"{'ID':>3}  {'Nombre':<20} {'RFID':<10} {'PIN':<4} {'Huella':<6}")
            for u in db.listar_usuarios(con):
                print(f"{u['id']:>3}  {u['nombre']:<20} {u['rfid_uid'] or '-':<10} "
                      f"{'sí' if u['tiene_pin'] else '-':<4} {u['huella_id'] if u['huella_id'] is not None else '-':<6}")

        elif cmd == "logs":
            n = int(args[0]) if args else 30
            for r in reversed(db.ultimos_logs(con, n)):
                print(f"{r['fecha']}  {r['metodo']:<8} {r['resultado']:<20} {r['usuario'] or '':<15} {r['detalle'] or ''}")

        elif cmd == "abrir":
            print(ipc.enviar({"tipo": "abrir_remoto", "usuario": "admin"}))

        elif cmd == "estado":
            print(ipc.enviar({"tipo": "estado"}))

        elif cmd == "web-admin":
            pw = getpass.getpass("Contraseña (mínimo 8 caracteres): ")
            if len(pw) < 8:
                sys.exit("Contraseña demasiado corta")
            if getpass.getpass("Repita la contraseña: ") != pw:
                sys.exit("No coinciden")
            db.crear_admin(con, args[0], pw)
            print(f"Administrador web '{args[0]}' guardado")

        elif cmd == "medir":
            import statistics
            datos = db.latencias(con, int(args[0]) if args else 200)
            if not datos:
                sys.exit("No hay aperturas con latencia registrada todavía")
            ms = sorted(v for _, v in datos)
            print(f"Aperturas medidas: {len(ms)}")
            print(f"Mediana: {statistics.median(ms):.1f} ms   Máxima: {ms[-1]:.1f} ms   Mínima: {ms[0]:.1f} ms")
            for metodo in sorted({m for m, _ in datos}):
                v = sorted(x for m, x in datos if m == metodo)
                print(f"  {metodo:<7} n={len(v):<4} mediana {statistics.median(v):7.1f} ms   máx {v[-1]:7.1f} ms")
            print(f"Requisito del Avance 1: < 3000 ms -> {'CUMPLE' if ms[-1] < 3000 else 'NO CUMPLE'}")

        else:
            sys.exit(__doc__)
    except IndexError:
        sys.exit(__doc__)
    except sqlite3.IntegrityError as e:
        sys.exit(f"Dato repetido: {e}")


if __name__ == "__main__":
    main()