obj-m += hola.o
obj-m += parametros.o
obj-m += p6buf.o
obj-m += p6cnt.o

KDIR ?= /lib/modules/$(shell uname -r)/build
all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

