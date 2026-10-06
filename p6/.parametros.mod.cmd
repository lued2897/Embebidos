savedcmd_parametros.mod := printf '%s\n'   parametros.o | awk '!x[$$0]++ { print("./"$$0) }' > parametros.mod
