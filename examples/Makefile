CC=gcc
CFLAGS=-O2 -Wall -g

.PHONY: all

all: sampleUSB

sampleUSB: sampleUSB.c
	$(CC) -o $@ $< $(CFLAGS) -lpthread
