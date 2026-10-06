savedcmd_p6buf.mod := printf '%s\n'   p6buf.o | awk '!x[$$0]++ { print("./"$$0) }' > p6buf.mod
