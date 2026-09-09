#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

void mostrar_linea(FILE *f, const char *etiqueta)
{
    char linea[256];
    unsigned long mb;

    while (fgets(linea, sizeof(linea), f))
    {
        if (strncmp(linea, etiqueta, strlen(etiqueta)) == 0)
        {
            sscanf(linea, "%*s %lu", &mb); 
	    printf("%s: %lu MB\n", etiqueta, mb / 1024); 
	    break;
            }
    }
}

int main(void)
{
    struct utsname info;

    uname(&info);

    printf("=== Reporte del sistema ===\n");
    printf("Arquitectura: %s\n", info.machine);
    printf("Kernel:       %s\n", info.release);

    FILE *cpu = fopen("/proc/cpuinfo", "r");

    if (cpu)
    {
        printf("\n-- CPU --\n");

        mostrar_linea(cpu, "Model");
        mostrar_linea(cpu, "Hardware");

        fclose(cpu);
    }

    FILE *mem = fopen("/proc/meminfo", "r");

    if (mem)
    {
        printf("\n-- Memoria --\n");

        mostrar_linea(mem, "MemTotal");
        mostrar_linea(mem, "MemFree");

        fclose(mem);
    }

    return 0;
}
