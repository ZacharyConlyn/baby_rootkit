obj-m := baby_kit.o
CC = gcc -Wall --std=c99
KDIR = /lib/modules/$(shell uname -r)/build
PWD = $(shell pwd)


modules:
	$(MAKE) -C $(KDIR)  M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
