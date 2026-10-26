/* prueba_logica.c -- pruebas automaticas del Reto Radiografia. NO MODIFICAR.
 * Solo prueba radiografia_logica.h con textos en memoria: no necesita Pi. */
#include <stdio.h>
#include <string.h>
#include "radiografia_cfg.h"
#include "radiografia_logica.h"

static int total = 0, ok = 0;
#define CHECK(desc, cond) do { total++; if (cond) ok++; else printf("  FALLA: %s\n", desc); } while (0)
#define SECCION(s) printf("[%s]\n", s)

int main(void)
{
    int a, b;
    char buf[64];
    long v;

    SECCION("version_kernel");
    CHECK("6.6.51+rpt-rpi-v8", version_kernel("6.6.51+rpt-rpi-v8", &a, &b) == 0 && a == 6 && b == 6);
    CHECK("5.15.0-91-generic", version_kernel("5.15.0-91-generic", &a, &b) == 0 && a == 5 && b == 15);
    CHECK("6.1.21+", version_kernel("6.1.21+", &a, &b) == 0 && a == 6 && b == 1);
    CHECK("solo '6.12'", version_kernel("6.12", &a, &b) == 0 && a == 6 && b == 12);
    CHECK("'6' solo -> -1", version_kernel("6", &a, &b) == -1);
    CHECK("'6.x' -> -1", version_kernel("6.x", &a, &b) == -1);
    CHECK("texto -> -1", version_kernel("desconocido", &a, &b) == -1);
    CHECK("vacio -> -1", version_kernel("", &a, &b) == -1);

    SECCION("campo_texto");
    CHECK("Name con tab", campo_texto("Name:\tbash\nState:\tS\n", "Name", buf, sizeof buf) == 0 && !strcmp(buf, "bash"));
    CHECK("segundo campo", campo_texto("Name:\tbash\nState:\tS (sleeping)\n", "State", buf, sizeof buf) == 0 && !strcmp(buf, "S (sleeping)"));
    CHECK("espacios al inicio", campo_texto("Campo:      valor\n", "Campo", buf, sizeof buf) == 0 && !strcmp(buf, "valor"));
    CHECK("espacios al final", campo_texto("Campo: valor   \n", "Campo", buf, sizeof buf) == 0 && !strcmp(buf, "valor"));
    CHECK("ultima linea sin \\n", campo_texto("A: 1\nB: dos", "B", buf, sizeof buf) == 0 && !strcmp(buf, "dos"));
    CHECK("no existe -> -1", campo_texto("A: 1\n", "B", buf, sizeof buf) == -1);
    CHECK("nombre exacto (Mem no es MemTotal)", campo_texto("MemTotal: 5\n", "Mem", buf, sizeof buf) == -1);
    CHECK("no confunde con valor", campo_texto("X: Name: hola\n", "Name", buf, sizeof buf) == -1);
    CHECK("no cabe -> -1", campo_texto("A: abcdefghij\n", "A", buf, 5) == -1);
    CHECK("cabe justo", campo_texto("A: abcd\n", "A", buf, 5) == 0 && !strcmp(buf, "abcd"));
    CHECK("parentesis en nombre", campo_texto("Active: 1\nActive(anon): 7\n", "Active(anon)", buf, sizeof buf) == 0 && !strcmp(buf, "7"));

    SECCION("campo_kb");
    CHECK("MemTotal", campo_kb("MemTotal:        3880112 kB\nMemFree: 5 kB\n", "MemTotal") == 3880112);
    CHECK("MemFree", campo_kb("MemTotal:        3880112 kB\nMemFree:    2961300 kB\n", "MemFree") == 2961300);
    CHECK("sin unidad", campo_kb("Threads:\t4\n", "Threads") == 4);
    CHECK("cero valido", campo_kb("SwapFree:   0 kB\n", "SwapFree") == 0);
    CHECK("no existe -> -1", campo_kb("MemTotal: 5 kB\n", "Buffers") == -1);
    CHECK("no numerico -> -1", campo_kb("Name:\tbash\n", "Name") == -1);
    CHECK("Active vs Active(anon)", campo_kb("Active(anon):  9 kB\nActive:  410 kB\n", "Active") == 410);

    SECCION("memoria_usada_kb");
    CHECK("total - disponible", memoria_usada_kb("MemTotal: 1000 kB\nMemFree: 100 kB\nMemAvailable: 600 kB\n") == 400);
    CHECK("orden distinto", memoria_usada_kb("MemAvailable: 250 kB\nMemTotal: 1000 kB\n") == 750);
    CHECK("sin MemAvailable -> -1", memoria_usada_kb("MemTotal: 1000 kB\nMemFree: 100 kB\n") == -1);
    CHECK("sin MemTotal -> -1", memoria_usada_kb("MemAvailable: 100 kB\n") == -1);
    CHECK("texto vacio -> -1", memoria_usada_kb("") == -1);

    SECCION("contar_cpus");
    CHECK("cuatro nucleos", contar_cpus("processor\t: 0\nBogoMIPS: 1\n\nprocessor\t: 1\n\nprocessor\t: 2\n\nprocessor\t: 3\n\nModel : Pi\n") == 4);
    CHECK("un nucleo", contar_cpus("processor : 0\nmodel name : ARMv6\n") == 1);
    CHECK("ignora 'processor' en otra linea", contar_cpus("model name : processor x\nprocessor\t: 0\n") == 1);
    CHECK("ignora prefijo de otra palabra", contar_cpus("processors : 8\nprocessor: 0\n") == 1);
    CHECK("sin cpus", contar_cpus("Hardware : BCM2711\n") == 0);
    CHECK("vacio", contar_cpus("") == 0);

    SECCION("contar_modulos");
    CHECK("tres modulos", contar_modulos("a 1 0 - Live 0x0\nb 2 1 - Live 0x1\nc 3 0 - Live 0x2\n") == 3);
    CHECK("sin \\n final", contar_modulos("a 1 0 - Live 0x0\nb 2 1 - Live 0x1") == 2);
    CHECK("ignora lineas vacias", contar_modulos("a 1 0 - Live 0x0\n\n   \nb 2 1 - Live 0x1\n") == 2);
    CHECK("vacio", contar_modulos("") == 0);
    CHECK("solo saltos", contar_modulos("\n\n\n") == 0);

    SECCION("cmdline_valor");
    {
        const char *c = "console=ttyS0,115200 console=tty1 root=PARTUUID=ea7d04d6-02 rootfstype=ext4 rootwait quiet\n";
        CHECK("root", cmdline_valor(c, "root", buf, sizeof buf) == 0 && !strcmp(buf, "PARTUUID=ea7d04d6-02"));
        CHECK("rootfstype", cmdline_valor(c, "rootfstype", buf, sizeof buf) == 0 && !strcmp(buf, "ext4"));
        CHECK("primera aparicion", cmdline_valor(c, "console", buf, sizeof buf) == 0 && !strcmp(buf, "ttyS0,115200"));
        CHECK("bandera sin valor", cmdline_valor(c, "quiet", buf, sizeof buf) == 0 && !strcmp(buf, ""));
        CHECK("rootwait es bandera", cmdline_valor(c, "rootwait", buf, sizeof buf) == 0 && buf[0] == '\0');
        CHECK("no existe -> -1", cmdline_valor(c, "init", buf, sizeof buf) == -1);
        CHECK("prefijo no basta (roo)", cmdline_valor(c, "roo", buf, sizeof buf) == -1);
        CHECK("clave mas larga (rootfstypex)", cmdline_valor(c, "rootfstypex", buf, sizeof buf) == -1);
        CHECK("valor con '='", cmdline_valor("a=b=c d=e", "a", buf, sizeof buf) == 0 && !strcmp(buf, "b=c"));
        CHECK("rootfstype primero", cmdline_valor("rootfstype=ext4 root=/dev/mmcblk0p2 rw", "root", buf, sizeof buf) == 0 && !strcmp(buf, "/dev/mmcblk0p2"));
        CHECK("espacios multiples y tab", cmdline_valor("  x=1 \t y=2  ", "y", buf, sizeof buf) == 0 && !strcmp(buf, "2"));
        CHECK("no cabe -> -1", cmdline_valor("root=0123456789", "root", buf, 5) == -1);
        CHECK("vacio -> -1", cmdline_valor("", "root", buf, sizeof buf) == -1);
    }

    SECCION("indice_mayor");
    {
        long a1[] = { 5, 9, 3 }, a2[] = { 7, 7, 2 }, a3[] = { -1, -1 }, a4[] = { -1, 0, -1 }, a5[] = { 4 };
        CHECK("mayor al centro", indice_mayor(a1, 3) == 1);
        CHECK("empate: el primero", indice_mayor(a2, 3) == 0);
        CHECK("solo negativos -> -1", indice_mayor(a3, 2) == -1);
        CHECK("cero cuenta", indice_mayor(a4, 3) == 1);
        CHECK("un elemento", indice_mayor(a5, 1) == 0);
        CHECK("n = 0 -> -1", indice_mayor(a1, 0) == -1);
    }

    SECCION("tipos_fs");
    {
        char d[160];
        const char *m = "sysfs /sys sysfs rw 0 0\nproc /proc proc rw 0 0\ntmpfs /run tmpfs rw 0 0\n"
                        "/dev/mmcblk0p2 / ext4 rw 0 0\ntmpfs /dev/shm tmpfs rw 0 0\n/dev/mmcblk0p1 /boot vfat rw 0 0\n";
        CHECK("lista sin repetir", tipos_fs(m, d, sizeof d) == 5 && !strcmp(d, "sysfs,proc,tmpfs,ext4,vfat"));
        CHECK("una sola linea", tipos_fs("a /b ext4 rw 0 0\n", d, sizeof d) == 1 && !strcmp(d, "ext4"));
        CHECK("sin \\n final", tipos_fs("a /b ext4 rw 0 0\nc /d vfat rw 0 0", d, sizeof d) == 2 && !strcmp(d, "ext4,vfat"));
        CHECK("vacio", tipos_fs("", d, sizeof d) == 0 && d[0] == '\0');
        CHECK("lineas incompletas se ignoran", tipos_fs("basura\na /b ext4 rw 0 0\n", d, sizeof d) == 1 && !strcmp(d, "ext4"));
        CHECK("no cabe -> -1", tipos_fs(m, d, 8) == -1);
    }

    SECCION("formato_uptime");
    {
        char d[32];
        CHECK("cero", formato_uptime(0, d, sizeof d) > 0 && !strcmp(d, "0d 00:00:00"));
        CHECK("5 segundos", formato_uptime(5.9, d, sizeof d) > 0 && !strcmp(d, "0d 00:00:05"));
        CHECK("un minuto", formato_uptime(60, d, sizeof d) > 0 && !strcmp(d, "0d 00:01:00"));
        CHECK("2 h 15 m 30 s", formato_uptime(8130.42, d, sizeof d) > 0 && !strcmp(d, "0d 02:15:30"));
        CHECK("1 dia 2:03:04", formato_uptime(93784.55, d, sizeof d) > 0 && !strcmp(d, "1d 02:03:04"));
        CHECK("exactamente un dia", formato_uptime(86400, d, sizeof d) > 0 && !strcmp(d, "1d 00:00:00"));
        CHECK("12 dias", formato_uptime(12 * 86400 + 3661, d, sizeof d) > 0 && !strcmp(d, "12d 01:01:01"));
        CHECK("negativo -> -1", formato_uptime(-1, d, sizeof d) == -1);
    }

    SECCION("formatear_reporte");
    {
        reporte_t r;
        char d[1024];
        const char *esp =
            "modelo=Pi X\nkernel=6.6.51+rpt-rpi-v8\nversion=6.6\ncpus=4\nmem_total_kb=3880112\n"
            "mem_usada_kb=574692\nuptime=0d 02:15:30\nmodulos=46\nroot=PARTUUID=ea7d04d6-02\n"
            "proceso_top=702:python3:48212\nfs_tipos=sysfs,proc,ext4\n";
        memset(&r, 0, sizeof r);
        strcpy(r.modelo, "Pi X"); strcpy(r.kernel, "6.6.51+rpt-rpi-v8"); r.mayor = 6; r.menor = 6;
        r.cpus = 4; r.mem_total_kb = 3880112; r.mem_usada_kb = 574692; strcpy(r.uptime, "0d 02:15:30");
        r.modulos = 46; strcpy(r.raiz, "PARTUUID=ea7d04d6-02"); r.pid_top = 702;
        strcpy(r.nombre_top, "python3"); r.rss_top_kb = 48212; strcpy(r.fs_tipos, "sysfs,proc,ext4");
        v = formatear_reporte(d, sizeof d, &r);
        CHECK("texto exacto", v > 0 && !strcmp(d, esp));
        CHECK("devuelve la longitud", v == (long)strlen(esp));
        CHECK("buffer chico -> -1", formatear_reporte(d, 20, &r) == -1);
        CHECK("buffer justo -> -1 (falta el \\0)", formatear_reporte(d, strlen(esp), &r) == -1);
        CHECK("buffer exacto + 1", formatear_reporte(d, strlen(esp) + 1, &r) == (int)strlen(esp));
    }

    printf("\nResultado: %d de %d pruebas correctas\n", ok, total);
    return ok == total ? 0 : 1;
}
