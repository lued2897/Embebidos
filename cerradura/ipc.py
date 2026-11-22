"""Mecanismos IPC del sistema.

- Socket UNIX (stream): entradas/web -> control. 
- Cola de mensajes POSIX: reed switch -> control (estado de la puerta).
- Cola de mensajes POSIX: control -> entradas (registrar o borrar una huella en el AS608).
"""
import json
import socket

import config


def enviar(msg: dict, timeout=5.0) -> dict:
    """Envía un mensaje JSON al proceso de control y espera su respuesta."""
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect(config.SOCKET_PATH)
        s.sendall((json.dumps(msg) + "\n").encode())
        datos = b""
        while not datos.endswith(b"\n"):
            parte = s.recv(1024)
            if not parte:
                break
            datos += parte
        return json.loads(datos) if datos else {"ok": False, "error": "sin respuesta"}
    except (OSError, json.JSONDecodeError) as e:
        return {"ok": False, "error": str(e)}
    finally:
        s.close()


def leer_mensaje(conn) -> dict:
    datos = b""
    while not datos.endswith(b"\n"):
        parte = conn.recv(1024)
        if not parte:
            break
        datos += parte
    return json.loads(datos)


def responder(conn, msg: dict):
    conn.sendall((json.dumps(msg) + "\n").encode())


def cola_puerta(crear=True):
    import posix_ipc
    flags = posix_ipc.O_CREAT if crear else 0
    return posix_ipc.MessageQueue(config.MQ_PUERTA, flags, max_messages=10, max_message_size=64)


def cola_ordenes():
    import posix_ipc
    return posix_ipc.MessageQueue(config.MQ_ORDENES, posix_ipc.O_CREAT, max_messages=10, max_message_size=256)


def enviar_orden(orden: dict):
    """Deja una orden para el proceso de entradas. La cola la guarda aunque entradas
    esté ocupado o reiniciándose; si está llena (10 órdenes) lanza BusyError."""
    cola = cola_ordenes()
    try:
        cola.send(json.dumps(orden), timeout=0)
    finally:
        cola.close()


def recibir_orden(cola, espera=0):
    """Siguiente orden de la cola o None si no hay (espera=0: no bloquea)."""
    import posix_ipc
    try:
        msg, _ = cola.receive(timeout=espera)
    except posix_ipc.BusyError:
        return None
    return json.loads(msg)
