/* radiografia.c -- Reto Radiografia (FSE 1858-6). NO MODIFICAR.
 * Lee archivos de /proc y arma el reporte con las funciones de
 * radiografia_logica.h.  Uso:  ./radiografia [--raiz DIRECTORIO]
 * Con --raiz lee <DIRECTORIO>/proc/... en lugar de /proc/... (pruebas). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "radiografia_cfg.h"
#include "radiografia_logica.h"

static const char *raiz = "";

/* Lee un archivo completo a buf (termina en '\0'). Devuelve bytes o -1. */
static long leer(const char *ruta, char *buf, size_t n)
{
    char full[512];
    FILE *f;
    size_t got;
    snprintf(full, sizeof full, "%s%s", raiz, ruta);
    f = fopen(full, "rb");
    if (!f) return -1;
    got = fread(buf, 1, n - 1, f);
    fclose(f);
    buf[got] = '\0';
    return (long)got;
}

static void copiar(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src);
}

int main(int argc, char **argv)
{
    static char meminfo[ARCHIVO_LIMITE], cpuinfo[ARCHIVO_LIMITE],
                modulos[ARCHIVO_LIMITE], mounts[ARCHIVO_LIMITE],
                cmdline[4096], uptime_txt[128], release[128], modelo[128],
                status[8192];
    reporte_t r;
    long rss[MAX_PROCESOS];
    int  pid[MAX_PROCESOS];
    char nombre[MAX_PROCESOS][32];
    int  np = 0, i;
    double seg = 0;
    char out[2048], tmp[160];
    DIR *d;
    struct dirent *e;

    if (argc == 3 && strcmp(argv[1], "--raiz") == 0) raiz = argv[2];
    else if (argc != 1) { fprintf(stderr, "uso: %s [--raiz DIR]\n", argv[0]); return 2; }

    memset(&r, 0, sizeof r);
    r.pid_top = -1; r.rss_top_kb = -1;
    meminfo[0] = cpuinfo[0] = modulos[0] = mounts[0] = cmdline[0] =
    uptime_txt[0] = release[0] = modelo[0] = '\0';

    leer("/proc/meminfo", meminfo, sizeof meminfo);
    leer("/proc/cpuinfo", cpuinfo, sizeof cpuinfo);
    leer("/proc/modules", modulos, sizeof modulos);
    leer("/proc/mounts", mounts, sizeof mounts);
    leer("/proc/cmdline", cmdline, sizeof cmdline);
    leer("/proc/uptime", uptime_txt, sizeof uptime_txt);
    leer("/proc/sys/kernel/osrelease", release, sizeof release);
    leer("/proc/device-tree/model", modelo, sizeof modelo);

    copiar(r.modelo, sizeof r.modelo, modelo[0] ? modelo : "desconocido");
    for (i = (int)strlen(release); i > 0 && isspace((unsigned char)release[i - 1]); i--) release[i - 1] = '\0';
    copiar(r.kernel, sizeof r.kernel, release[0] ? release : "desconocido");
    if (version_kernel(release, &r.mayor, &r.menor) != 0) r.mayor = r.menor = -1;
    r.cpus = contar_cpus(cpuinfo);
    r.mem_total_kb = campo_kb(meminfo, "MemTotal");
    r.mem_usada_kb = memoria_usada_kb(meminfo);
    if (sscanf(uptime_txt, "%lf", &seg) != 1 || formato_uptime(seg, r.uptime, sizeof r.uptime) < 0)
        copiar(r.uptime, sizeof r.uptime, "desconocido");
    r.modulos = contar_modulos(modulos);
    if (cmdline_valor(cmdline, "root", r.raiz, sizeof r.raiz) != 0)
        copiar(r.raiz, sizeof r.raiz, "(ninguno)");

    {
        char dir[512];
        snprintf(dir, sizeof dir, "%s/proc", raiz);
        d = opendir(dir);
    }
    while (d && (e = readdir(d)) && np < MAX_PROCESOS) {
        char ruta[300], nm[64];
        int ok = 1;
        for (i = 0; e->d_name[i]; i++)
            if (!isdigit((unsigned char)e->d_name[i])) ok = 0;
        if (!ok || !e->d_name[0]) continue;
        snprintf(ruta, sizeof ruta, "/proc/%s/status", e->d_name);
        if (leer(ruta, status, sizeof status) < 0) continue;
        pid[np] = atoi(e->d_name);
        rss[np] = campo_kb(status, "VmRSS");
        if (campo_texto(status, "Name", nm, sizeof nm) != 0) strcpy(nm, "?");
        copiar(nombre[np], sizeof nombre[np], nm);
        np++;
    }
    if (d) closedir(d);
    /* orden estable por PID para que el empate sea reproducible */
    for (i = 1; i < np; i++) {
        int j = i, p = pid[i]; long q = rss[i]; char nm[32];
        strcpy(nm, nombre[i]);
        while (j > 0 && pid[j - 1] > p) {
            pid[j] = pid[j - 1]; rss[j] = rss[j - 1]; strcpy(nombre[j], nombre[j - 1]);
            j--;
        }
        pid[j] = p; rss[j] = q; strcpy(nombre[j], nm);
    }
    i = indice_mayor(rss, np);
    if (i >= 0) { r.pid_top = pid[i]; copiar(r.nombre_top, sizeof r.nombre_top, nombre[i]); r.rss_top_kb = rss[i]; }
    else copiar(r.nombre_top, sizeof r.nombre_top, "-");

    if (tipos_fs(mounts, tmp, sizeof tmp) < 0) copiar(tmp, sizeof tmp, "demasiados");
    copiar(r.fs_tipos, sizeof r.fs_tipos, tmp);

    if (formatear_reporte(out, sizeof out, &r) < 0) {
        fprintf(stderr, "formatear_reporte: no cabe o fallo\n");
        return 1;
    }
    fputs(out, stdout);
    return 0;
}
