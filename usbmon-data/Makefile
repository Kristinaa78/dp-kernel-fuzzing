CC=gcc
CFLAGS=-O2 -Wall -g

.PHONY: all

all: usbmon

usbmon: usbmon.c
	$(CC) -o $@ $< $(CFLAGS) -lpthread
