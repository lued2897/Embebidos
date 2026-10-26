/* radiografia_logica.h  --  AQUI TRABAJA EL EQUIPO
 *
 * Implementen cada funcion. Todas reciben TEXTO ya leido (el programa
 * principal abre los archivos); ninguna debe abrir archivos ni imprimir.
 * Descripcion completa de cada una: radiografia_cfg.h y la guia del reto.
 *
 * Funciones de la biblioteca estandar que ayudan:
 *   strchr, strlen, strncmp, strcmp, memcpy, strtol, snprintf, sscanf,
 *   isdigit, isspace.
 *
 * Meta: ./prueba_logica  ->  "Resultado: N de N pruebas correctas".
 * Con las funciones vacias (como llegan) solo pasan unas pocas.
 */
#ifndef RADIOGRAFIA_LOGICA_H
#define RADIOGRAFIA_LOGICA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "radiografia_cfg.h"

/* "6.6.51+rpt-rpi-v8" -> *mayor = 6, *menor = 6, devuelve 0.
 * Si el texto no empieza con "numero.numero", devuelve -1. */
static int version_kernel(const char *release, int *mayor, int *menor)
{
    if (!release || !mayor || !menor) return -1;
    if (sscanf(release, "%d.%d", mayor, menor) == 2) {
        return 0;
    }
    return -1;
}

/* Busca una linea que EMPIECE con  campo  seguido de ':'  y copia el valor
 * (sin espacios ni tabuladores al inicio ni al final) en dst.
 * Ejemplo:  "Name:\tbash\n"  con campo "Name"  ->  "bash".
 * Devuelve 0 si lo encuentra y cabe en n bytes (con el '\0'); -1 si no. */
static int campo_texto(const char *texto, const char *campo, char *dst, size_t n)
{
    if (!texto || !campo || !dst || n == 0) return -1;
    
    size_t len_campo = strlen(campo);
    const char *p = texto;
    
    while (p && *p) {
        if (strncmp(p, campo, len_campo) == 0) {
            const char *q = p + len_campo;
            
            /* Saltar espacios despues del campo */
            while (*q == ' ' || *q == '\t') q++;
            
            if (*q == ':') {
                q++;
                /* Saltar espacios antes del valor */
                while (*q == ' ' || *q == '\t') q++;
                
                const char *fin = q;
                while (*fin && *fin != '\n') fin++;
                
                /* Retroceder espacios al final */
                const char *fin_real = fin;
                while (fin_real > q && isspace((unsigned char)*(fin_real - 1))) {
                    fin_real--;
                }
                
                size_t longitud_valor = fin_real - q;
                if (longitud_valor >= n) return -1;
                
                memcpy(dst, q, longitud_valor);
                dst[longitud_valor] = '\0';
                return 0;
            }
        }
        p = strchr(p, '\n');
        if (p) p++; /* Avanzar a la siguiente linea */
    }
    
    return -1;
}

/* Igual que campo_texto, pero devuelve el numero. "MemTotal:  3880112 kB"
 * -> 3880112. Devuelve -1 si el campo no existe o su valor no es un numero. */
static long campo_kb(const char *texto, const char *campo)
{
    char buf[128];
    if (campo_texto(texto, campo, buf, sizeof(buf)) == 0) {
        char *endptr;
        long valor = strtol(buf, &endptr, 10);
        /* Se asume exito si logro extraer digitos al inicio */
        if (endptr != buf) {
            return valor;
        }
    }
    return -1;
}

/* MemTotal - MemAvailable de un texto tipo /proc/meminfo. -1 si falta alguno. */
static long memoria_usada_kb(const char *meminfo)
{
    long total = campo_kb(meminfo, "MemTotal");
    long disponible = campo_kb(meminfo, "MemAvailable");
    
    if (total >= 0 && disponible >= 0) {
        return total - disponible;
    }
    return -1;
}

/* Cantidad de lineas de /proc/cpuinfo que empiezan con "processor" y luego
 * (con espacios o tabuladores opcionales) un ':'. */
static int contar_cpus(const char *cpuinfo)
{
    if (!cpuinfo) return 0;
    
    int cpus = 0;
    const char *p = cpuinfo;
    
    while (p && *p) {
        if (strncmp(p, "processor", 9) == 0) {
            const char *q = p + 9;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == ':') {
                cpus++;
            }
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    
    return cpus;
}

/* Cantidad de lineas NO vacias de /proc/modules (una por modulo cargado). */
static int contar_modulos(const char *proc_modules)
{
    if (!proc_modules) return 0;
    
    int modulos = 0;
    const char *p = proc_modules;
    
    while (p && *p) {
        const char *fin = strchr(p, '\n');
        int tiene_contenido = 0;
        
        for (const char *q = p; q != fin && *q != '\0'; q++) {
            if (!isspace((unsigned char)*q)) {
                tiene_contenido = 1;
                break;
            }
        }
        
        if (tiene_contenido) {
            modulos++;
        }
        
        if (fin) p = fin + 1;
        else break;
    }
    
    return modulos;
}

/* En una linea de comandos del kernel (palabras separadas por espacios),
 * busca la PRIMERA palabra "clave=valor" y copia el valor en dst.
 *   "root=PARTUUID=ea7d04d6-02 rootwait" , "root"      -> "PARTUUID=ea7d04d6-02"
 *   "root=PARTUUID=ea7d04d6-02 rootwait" , "rootwait"  -> "" (bandera sin valor)
 * La clave debe coincidir completa ("root" no es "rootfstype").
 * Devuelve 0 si la encuentra y cabe; -1 si no. */
static int cmdline_valor(const char *cmdline, const char *clave, char *dst, size_t n)
{
    if (!cmdline || !clave || !dst || n == 0) return -1;
    
    size_t len_clave = strlen(clave);
    const char *p = cmdline;
    
    while (*p) {
        /* Omitir espacios iniciales */
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        
        if (strncmp(p, clave, len_clave) == 0) {
            char next = p[len_clave];
            if (next == '=' || next == ' ' || next == '\t' || next == '\n' || next == '\0') {
                if (next == '=') {
                    p += len_clave + 1;
                    const char *fin = p;
                    while (*fin && *fin != ' ' && *fin != '\t' && *fin != '\n') fin++;
                    
                    size_t long_valor = fin - p;
                    if (long_valor >= n) return -1;
                    
                    memcpy(dst, p, long_valor);
                    dst[long_valor] = '\0';
                } else {
                    dst[0] = '\0';
                }
                return 0;
            }
        }
        /* Avanzar a la siguiente palabra */
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    }
    
    return -1;
}

/* Indice del mayor valor de v[0..n-1] considerando solo valores >= 0.
 * En empate gana el primero. Si no hay ninguno >= 0 (o n <= 0) devuelve -1. */
static int indice_mayor(const long *v, int n)
{
    if (!v || n <= 0) return -1;
    
    int id_mayor = -1;
    long mayor = -1;
    
    for (int i = 0; i < n; i++) {
        if (v[i] >= 0 && v[i] > mayor) {
            mayor = v[i];
            id_mayor = i;
        }
    }
    
    return id_mayor;
}

/* De un texto tipo /proc/mounts toma la 3a columna de cada linea (el tipo de
 * sistema de archivos) y escribe en dst los tipos SIN REPETIR, en orden de
 * aparicion, separados por comas:  "sysfs,proc,tmpfs,ext4".
 * Devuelve cuantos tipos distintos hubo, o -1 si no cupo en n bytes. */
static int tipos_fs(const char *proc_mounts, char *dst, size_t n)
{
    if (!proc_mounts || !dst || n == 0) return -1;
    
    const char *p = proc_mounts;
    char buffer[1024] = "";
    size_t buf_len = 0;
    int cantidad = 0;
    
    /* Para almacenar temporalmente y comparar sin usar strstr */
    char encontrados[128][32];
    
    while (p && *p) {
        /* Saltar 1ra columna (device) */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || !*p) goto sig_linea;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        
        /* Saltar 2da columna (mount point) */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || !*p) goto sig_linea;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        
        /* Capturar 3ra columna (filesystem) */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || !*p) goto sig_linea;
        
        const char *inicio_fs = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        
        size_t fs_len = p - inicio_fs;
        if (fs_len > 0 && fs_len < 32 && cantidad < 128) {
            char tipo_actual[32];
            memcpy(tipo_actual, inicio_fs, fs_len);
            tipo_actual[fs_len] = '\0';
            
            /* Revisar si ya esta en encontrados */
            int duplicado = 0;
            for (int i = 0; i < cantidad; i++) {
                if (strcmp(encontrados[i], tipo_actual) == 0) {
                    duplicado = 1;
                    break;
                }
            }
            
            if (!duplicado) {
                strcpy(encontrados[cantidad], tipo_actual);
                
                if (cantidad > 0) {
                    if (buf_len + 1 + fs_len >= sizeof(buffer)) return -1;
                    strcat(buffer, ",");
                    strcat(buffer, tipo_actual);
                    buf_len += fs_len + 1;
                } else {
                    if (fs_len >= sizeof(buffer)) return -1;
                    strcpy(buffer, tipo_actual);
                    buf_len = fs_len;
                }
                cantidad++;
            }
        }
        
    sig_linea:
        p = strchr(p, '\n');
        if (p) p++;
    }
    printf("%.*s\n", buf_len, buffer);
    if (buf_len >= n) return -1;
    strcpy(dst, buffer);
    return cantidad;
}

/* Segundos -> "Dd HH:MM:SS" (dias sin ceros a la izquierda, lo demas con 2
 * digitos, segundos truncados). 93784.55 -> "1d 02:03:04".
 * Devuelve los caracteres escritos o -1 si seg < 0. */
static int formato_uptime(double seg, char *dst, size_t n)
{
    if (seg < 0 || !dst || n == 0) return -1;
    
    long s_totales = (long)seg;
    long dias = s_totales / 86400;
    s_totales %= 86400;
    long horas = s_totales / 3600;
    s_totales %= 3600;
    long min = s_totales / 60;
    long sec = s_totales % 60;
    
    int escritos = snprintf(dst, n, "%ldd %02ld:%02ld:%02ld", dias, horas, min, sec);
    
    if (escritos >= 0 && (size_t)escritos < n) {
        return escritos;
    }
    return -1; /* Se truncó el buffer */
}

/* Escribe en dst el reporte, una linea por campo, en este orden y formato:
 *   modelo=...
 *   kernel=...
 *   version=MAYOR.MENOR
 *   cpus=...
 *   mem_total_kb=...
 *   mem_usada_kb=...
 *   uptime=...
 *   modulos=...
 *   root=...
 *   proceso_top=PID:NOMBRE:RSS_KB
 *   fs_tipos=...
 * (cada linea termina en '\n'; ejemplo en esperado_A.txt).
 * Devuelve los caracteres escritos, o -1 si no cupo (incluido el '\0'). */
static int formatear_reporte(char *dst, size_t n, const reporte_t *r)
{
    if (!dst || n == 0 || !r) return -1;
    
    int escritos = snprintf(dst, n,
        "modelo=%s\n"
        "kernel=%s\n"
        "version=%d.%d\n"
        "cpus=%d\n"
        "mem_total_kb=%ld\n"
        "mem_usada_kb=%ld\n"
        "uptime=%s\n"
        "modulos=%d\n"
        "root=%s\n"
        "proceso_top=%d:%s:%ld\n"
        "fs_tipos=%s\n",
        r->modelo,
        r->kernel,
        r->mayor, r->menor,
        r->cpus,
        r->mem_total_kb,
        r->mem_usada_kb,
        r->uptime,
        r->modulos,
        r->raiz,
        r->pid_top, r->nombre_top, r->rss_top_kb,
        r->fs_tipos
    );
    
    if (escritos >= 0 && (size_t)escritos < n) {
        return escritos;
    }
    return -1;
}

#endif