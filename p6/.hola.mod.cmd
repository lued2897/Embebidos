savedcmd_hola.mod := printf '%s\n'   hola.o | awk '!x[$$0]++ { print("./"$$0) }' > hola.mod
