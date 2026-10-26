#!/usr/bin/env bash
# campo.sh -- Mision de campo. Ejecutar en la Raspberry Pi real:
#     ./campo.sh
# Genera la carpeta campo_<fecha>/ con la salida de ./radiografia, la salida
# cruda de los comandos del sistema y un cotejo automatico entre ambas.
set -u
cd "$(dirname "$0")"
make >/dev/null 2>&1 || { echo "No compila: corrijan primero (make)"; exit 1; }

modelo=$(tr -d '\0' </proc/device-tree/model 2>/dev/null || true)
case "$modelo" in
    *Raspberry*) ;;
    *) if [ "${RADIOGRAFIA_PC:-0}" != 1 ]; then
           echo "Este equipo no parece una Raspberry Pi (modelo: '${modelo:-desconocido}')."
           echo "La mision de campo debe correr en la Pi."; exit 1
       fi ;;
esac

dir="campo_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$dir"
./radiografia > "$dir/radiografia.txt" || { echo "radiografia fallo"; exit 1; }
{
  echo "uname_r=$(uname -r)"
  echo "nproc=$(nproc --all)"
  echo "memtotal_kb=$(awk '/^MemTotal:/{print $2}' /proc/meminfo)"
  echo "modulos=$(lsmod | tail -n +2 | wc -l)"
  echo "root=$(tr ' ' '\n' </proc/cmdline | grep '^root=' | head -1 | cut -d= -f2-)"
  echo "modelo=$(tr -d '\0' </proc/device-tree/model 2>/dev/null)"
  echo "fs_distintos=$(awk '{print $3}' /proc/mounts | sort -u | wc -l)"
} > "$dir/crudo.txt"
{ echo "--- config.txt (sin comentarios) ---"
  grep -hv -e '^#' -e '^$' /boot/firmware/config.txt /boot/config.txt 2>/dev/null
  echo "--- cmdline ---"; cat /proc/cmdline
  echo "--- ls /boot/firmware ---"; ls -l /boot/firmware 2>/dev/null || ls -l /boot 2>/dev/null
  echo "--- dmesg (primeras lineas) ---"; (dmesg 2>/dev/null || sudo dmesg 2>/dev/null) | head -15
} > "$dir/arranque.txt" 2>&1
# Datos para verificar las preguntas de campo en la pagina del reto (examen.athlixapp.com/reto/equipo)
{ echo "--- lsblk ---"; lsblk -o NAME,PARTUUID,MOUNTPOINT 2>/dev/null
  echo "--- dev ---"; ls /dev/i2c* /dev/spidev* 2>/dev/null
  echo "--- config_activas ---"; grep -hE '^[[:space:]]*(dtparam|dtoverlay)' /boot/firmware/config.txt /boot/config.txt 2>/dev/null
  echo "--- lsmod_top3 ---"; lsmod | tail -n +2 | sort -k2 -n -r | head -3
  echo "--- ps_top3 ---"; ps -eo pid,comm,rss --sort=-rss | head -4
  echo "--- dmesg ---"; (dmesg 2>/dev/null || sudo dmesg 2>/dev/null) | grep -E 'EXT4-fs .*mounted filesystem|Kernel command line' | head -5
  echo "--- fin ---"
} > "$dir/preguntas.txt" 2>&1
if command -v strace >/dev/null 2>&1; then
    strace -f -c -o "$dir/strace.txt" ./radiografia >/dev/null 2>&1
fi

get() { grep "^$1=" "$2" | head -1 | cut -d= -f2-; }
pasa=0; total=0
chk() {  # descripcion  valor_programa  valor_crudo  tolerancia
    total=$((total+1))
    if [[ "$2" =~ ^-?[0-9]+$ && "$3" =~ ^-?[0-9]+$ ]]; then
        d=$(( $2 > $3 ? $2 - $3 : $3 - $2 ))
        if [ "$d" -le "${4:-0}" ]; then pasa=$((pasa+1)); echo "  OK         $1: $2"; else echo "  DIFERENTE  $1: programa=$2 sistema=$3"; fi
    else
        if [ "$2" = "$3" ]; then pasa=$((pasa+1)); echo "  OK         $1: $2"; else echo "  DIFERENTE  $1: programa='$2' sistema='$3'"; fi
    fi
}
R="$dir/radiografia.txt"; C="$dir/crudo.txt"
echo "Cotejo (programa vs comandos del sistema):" | tee "$dir/cotejo.txt"
{
chk "kernel"       "$(get kernel $R)"        "$(get uname_r $C)"
chk "cpus"         "$(get cpus $R)"          "$(get nproc $C)"
chk "mem_total_kb" "$(get mem_total_kb $R)"  "$(get memtotal_kb $C)"
chk "modulos"      "$(get modulos $R)"       "$(get modulos $C)" 2
chk "root"         "$(get root $R)"          "$(get root $C)"
chk "modelo"       "$(get modelo $R)"        "$(get modelo $C)"
n_fs=$(get fs_tipos $R | tr ',' '\n' | grep -c .)
chk "tipos de fs"  "$n_fs"                   "$(get fs_distintos $C)" 1
echo "PUNTOS DE CAMPO (cotejo): $((20 * pasa / total)) / 20  ($pasa de $total coincidencias)"
} | tee -a "$dir/cotejo.txt"
echo; echo "Carpeta generada: $dir/   (incluirla en la entrega)"
