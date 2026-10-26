#!/usr/bin/env bash
# evaluar.sh -- calificacion automatica del Reto Radiografia (partes que se
# pueden medir sin docente). Ejecutar dentro de la carpeta del reto:
#     ./evaluar.sh
set -u
cd "$(dirname "$0")"

echo "== 1. Compilacion =="
make clean >/dev/null 2>&1
if ! make 2>compilacion.log >/dev/null; then
    echo "NO COMPILA. Revisen compilacion.log"; cat compilacion.log | head -20
    echo; echo "PUNTOS AUTOMATICOS: 0 / 40"; exit 1
fi
avisos=$(grep -c 'warning:' compilacion.log || true)
echo "Compila. Avisos (warnings): $avisos"

echo; echo "== 2. Pruebas de la logica (prueba_logica) =="
salida=$(./prueba_logica)
echo "$salida" | grep -E 'FALLA|Resultado'
ok=$(echo "$salida" | sed -n 's/^Resultado: \([0-9]*\) de \([0-9]*\).*/\1/p')
tot=$(echo "$salida" | sed -n 's/^Resultado: \([0-9]*\) de \([0-9]*\).*/\2/p')
ok=${ok:-0}; tot=${tot:-1}

echo; echo "== 3. Reporte completo con la Raspberry Pi simulada A =="
./radiografia --raiz fixtures/A > salida_A.txt 2>/dev/null
lineas=$(wc -l < esperado_A.txt)
buenas=0
while IFS= read -r l; do
    if grep -qxF -- "$l" salida_A.txt; then buenas=$((buenas+1)); else echo "  DIFERENTE: esperado -> $l"; fi
done < esperado_A.txt
echo "Lineas correctas: $buenas de $lineas"

echo; echo "== 4. Calidad del codigo (desempate) =="
pedantic=$(gcc -std=c11 -Wall -Wextra -Wpedantic -fsyntax-only radiografia.c 2>&1 | grep -c 'warning:' || true)
echo "Avisos con -Wall -Wextra -Wpedantic: $pedantic   (menos es mejor; solo desempata)"

pts_prueba=$(( 30 * ok / tot ))
pts_rep=$(( 10 * buenas / lineas ))
echo; echo "=================================================="
echo "PUNTOS AUTOMATICOS: $((pts_prueba + pts_rep)) / 40   (pruebas $pts_prueba/30 + reporte $pts_rep/10)"
echo "AVISOS_PEDANTIC: $pedantic"
echo "=================================================="
