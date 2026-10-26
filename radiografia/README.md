# Reto Radiografia (FSE 1858-6)

Contenido
- radiografia_logica.h   <- UNICO archivo que se modifica (10 funciones + formato)
- radiografia_cfg.h      tipo del reporte y descripcion de las funciones (no modificar)
- radiografia.c          programa principal (no modificar)
- prueba_logica.c        pruebas automaticas (no modificar)
- fixtures/A/            una "Raspberry Pi" simulada: copia de /proc de una Pi 4
- esperado_A.txt         reporte que debe producir:  ./radiografia --raiz fixtures/A
- evaluar.sh             puntos automaticos (40)
- campo.sh               mision de campo en la Pi real (20)
- entrega.sh             empaqueta la entrega

Uso rapido
    make                     compila
    ./prueba_logica          pruebas de la logica
    ./radiografia --raiz fixtures/A | diff - esperado_A.txt
    ./evaluar.sh             puntos automaticos
    ./campo.sh               (en la Pi)
    ./entrega.sh 3           (3 = numero de equipo)

La logica se puede desarrollar en cualquier computadora con Linux, macOS o WSL
(gcc o clang). No se necesita Raspberry Pi hasta la mision de campo.
