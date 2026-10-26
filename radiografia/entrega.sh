#!/usr/bin/env bash
# entrega.sh NUMERO_DE_EQUIPO -- empaqueta lo que se entrega del reto.
#     ./entrega.sh 3     ->  radiografia_equipo3.tar.gz
set -u
cd "$(dirname "$0")"
[ $# -eq 1 ] || { echo "uso: ./entrega.sh NUMERO_DE_EQUIPO"; exit 2; }
ultimo=$(ls -d campo_* 2>/dev/null | tail -1)
[ -n "$ultimo" ] || { echo "Falta la mision de campo: ejecuten ./campo.sh en la Pi."; exit 1; }
./evaluar.sh > resultado_evaluar.txt 2>&1
tail -4 resultado_evaluar.txt
tar czf "radiografia_equipo$1.tar.gz" radiografia_logica.h resultado_evaluar.txt "$ultimo"
echo "Listo: radiografia_equipo$1.tar.gz  (suban este archivo junto con el reporte)"
