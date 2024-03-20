CC=gcc
CFLAGS=-O2 -Wall -g

.PHONY: all

all: rg-wrapper

rg-wrapper: rg-wrapper.c
	$(CC) -o $@ $< $(CFLAGS) -lpthread
