#include <stdio.h>
#include <string.h>

#define VENTANA 4
#define UMBRAL 2700
#define HISTERESIS 30
#define LECTURA_INVALIDA (-9999)

enum estado { VIGILANDO, ALARMA, PAUSADO, FALLA };

//static const char *nombre_estado[] = { "VIGILANDO", "ALARMA", "PAUSADO" };
static const char *nombre_estado[] = { "VIGILANDO", "ALARMA", "PAUSADO", "FALLA" };

struct ventana {
    int dato[VENTANA];
    int cuenta;
    int pos;
};

static void ventana_agregar(struct ventana *w, int lectura)
{
    w->dato[w->pos] = lectura;
    //w->pos = (w->pos + 1) % (VENTANA + 1); //defecto 1
    w->pos = (w->pos + 1) % (VENTANA);
    if (w->cuenta < VENTANA) {
        w->cuenta++;
    }
}

static int ventana_promedio(const struct ventana *w)
{
    long suma = 0;
    for (int i = 0; i < w->cuenta; i++) {
        suma += w->dato[i];
    }
    return (int)(suma / w->cuenta);
}

static enum estado estado_siguiente(enum estado actual, int temp, int boton, int valida)
{
    if (!valida) {
        return FALLA;
    }
    if (boton) {
        return actual == PAUSADO ? VIGILANDO : PAUSADO;
    }
    switch (actual) {
    case VIGILANDO:
        return temp >= UMBRAL ? ALARMA : VIGILANDO;
    case ALARMA:
        //return temp >= UMBRAL - HISTERESIS ? ALARMA : VIGILANDO;
        return temp > UMBRAL - HISTERESIS ? ALARMA : VIGILANDO;
    case PAUSADO:
        return PAUSADO;
    case FALLA:
        return valida ? VIGILANDO : FALLA;
    }
    return actual;
}

static void registrar(int t_s, int temp, enum estado e)
{
    char etiqueta[12];

    strcpy(etiqueta, nombre_estado[e]);
    printf("%3d s  %2d.%02d C  %s\n", t_s, temp / 100, temp % 100, etiqueta);
}

int main(void)
{
    /* Una lectura por segundo. -9999 es lectura invalida del sensor. */
    int entrada[] = { 2540, 2580, 2640, 2710, 2760, 2790, 2720, 2650, 2630, 2680,
                      2610, LECTURA_INVALIDA, 2600, 2610 };
    int n = (int)(sizeof(entrada) / sizeof(entrada[0]));
    struct ventana w;
    enum estado e = VIGILANDO;

    memset(&w, 0, sizeof(w));
    for (int t = 0; t < n; t++) {
        int valida = entrada[t] != LECTURA_INVALIDA;
        int prom = 0;

        if (valida) {
            ventana_agregar(&w, entrada[t]);
            prom = ventana_promedio(&w);
        }
        e = estado_siguiente(e, prom, 0, valida);
        registrar(t, prom, e);
    }
    return 0;
}
