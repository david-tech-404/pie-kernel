# Pie Kernel - OSP Init Loader Makefile
CC = arm-none-linux-gnueabihf-gcc
CFLAGS = -Wall -O2
LDFLAGS = -ldl

all: osp_init

osp_init: osp_init.c
	$(CC) $(CFLAGS) -o osp_init osp_init.c $(LDFLAGS)

clean:
	rm -f osp_init

.PHONY: all clean