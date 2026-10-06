#!/bin/bash
# Descarga los modulos de la practica 6 que sigan cargados
for m in p6cnt p6buf parametros hola; do
    if lsmod | grep -q "^$m "; then
        sudo rmmod "$m" && echo "descargado: $m"
    else
    echo "no estaba cargado: $m"
    fi
done
lsmod | grep -E "^(hola|parametros|p6buf|p6cnt) " || echo "ningun modulo de la practica 6 sigue cargado"
ls /dev/p6* 2>/dev/null || echo "sin dispositivos p6 en /dev"