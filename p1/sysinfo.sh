#!/bin/bash

LOG="sysinfo_$(date +%Y%m%d_%H%M%S).log"

{
    echo "========================================"
    echo "       REPORTE DE INFORMACIÓN"
    echo "========================================"
    echo "Reporte generado el: $(date)"
    echo

    echo "--------------- SISTEMA ----------------"
    uname -a
    echo

    echo "---------------- CPU -------------------"
    lscpu | head -n 10
    echo

    echo "--------------- MEMORIA ----------------"
    free -h
    echo

    echo "---------------- DISCO -----------------"
    df -h /
    echo

    echo "--------------- PROCESOS ---------------"
    ps aux --sort=-%cpu | head -n 6
    echo

    echo "========================================"

} | tee "$LOG"

echo
echo "Reporte guardado en: $LOG"
