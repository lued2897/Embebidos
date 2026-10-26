/* radiografia_cfg.h  --  Reto Radiografia (FSE 1858-6)
 * NO MODIFICAR. Define el tipo del reporte y la lista de funciones que el
 * equipo debe implementar en radiografia_logica.h. */
#ifndef RADIOGRAFIA_CFG_H
#define RADIOGRAFIA_CFG_H

#include <stddef.h>

#define MAX_PROCESOS 512
#define ARCHIVO_LIMITE 65536      /* bytes maximos que se leen de un archivo */

typedef struct {
    char modelo[64];       /* /proc/device-tree/model                    */
    char kernel[64];       /* /proc/sys/kernel/osrelease                 */
    int  mayor, menor;     /* version del kernel: 6.6.51+... -> 6 y 6    */
    int  cpus;             /* nucleos en /proc/cpuinfo                   */
    long mem_total_kb;     /* MemTotal                                   */
    long mem_usada_kb;     /* MemTotal - MemAvailable                    */
    char uptime[32];       /* /proc/uptime, formato "1d 02:03:04"        */
    int  modulos;          /* lineas de /proc/modules                    */
    char raiz[96];         /* valor de root= en /proc/cmdline            */
    int  pid_top;          /* proceso con mayor VmRSS                    */
    char nombre_top[32];
    long rss_top_kb;
    char fs_tipos[160];    /* tipos de sistema de archivos, sin repetir  */
} reporte_t;

/* ---- Funciones que implementa el equipo (radiografia_logica.h) ----------
 * Todas reciben TEXTO ya leido; ninguna abre archivos.
 *
 *  version_kernel   "6.6.51+rpt-rpi-v8" -> mayor=6, menor=6. 0 si hay dos numeros, -1 si no.
 *  campo_texto      Busca la linea "Campo:<espacios o tab>valor" y copia el valor
 *                   (sin espacios en los extremos). 0 si lo encuentra y cabe, -1 si no.
 *  campo_kb         Igual, pero devuelve el numero (en kB). -1 si no existe.
 *  memoria_usada_kb MemTotal - MemAvailable de un /proc/meminfo. -1 si falta alguno.
 *  contar_cpus      Lineas de /proc/cpuinfo que empiezan con "processor".
 *  contar_modulos   Lineas no vacias de /proc/modules.
 *  cmdline_valor    Valor de clave=valor en /proc/cmdline (primera aparicion).
 *  indice_mayor     Indice del mayor valor >= 0 (el primero si hay empate). -1 si no hay.
 *  tipos_fs         Tipos de /proc/mounts (3a columna), sin repetir, separados por comas.
 *  formato_uptime   Segundos -> "Dd HH:MM:SS".
 *  formatear_reporte  Escribe el reporte como lineas clave=valor.
 * ------------------------------------------------------------------------ */

#endif
