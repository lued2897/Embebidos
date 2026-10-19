#!/bin/bash
# Mide cuanto tiempo de CPU consume el programa `centinela` en una ventana de tiempo.
#   ./medir_cpu.sh [segundos] [nombre_del_proceso]      (por omision 60 y centinela)
# Ejecuta ./centinela en otra terminal antes de lanzar este script.

SEG="${1:-60}"
NOMBRE="${2:-centinela}"
PID="$(pgrep -n -x "$NOMBRE")"
if [ -z "$PID" ]; then
	echo "No hay ningun proceso llamado $NOMBRE en ejecucion" >&2
	exit 1
fi
TICKS="$(getconf CLK_TCK)"

# utime + stime (campos 14 y 15 de /proc/PID/stat), en ticks del reloj
cpu_ticks() {
	local resto
	resto="$(sed 's/^.*) //' "/proc/$PID/stat")"
	set -- $resto
	echo $(( ${12} + ${13} ))
}

A="$(cpu_ticks)" || exit 1
echo "Midiendo el proceso $PID durante $SEG s..."
sleep "$SEG"
B="$(cpu_ticks)" || { echo "El proceso termino durante la medicion" >&2; exit 1; }

MS=$(( (B - A) * 1000 / TICKS ))
echo "CPU consumida en $SEG s: $MS ms"
