"""Mecanismos IPC del sistema.

- Socket UNIX (stream): entradas/web -> control. 
- Cola de mensajes POSIX: reed switch -> control (estado de la puerta).
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
