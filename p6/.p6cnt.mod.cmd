savedcmd_p6cnt.mod := printf '%s\n'   p6cnt.o | awk '!x[$$0]++ { print("./"$$0) }' > p6cnt.mod
