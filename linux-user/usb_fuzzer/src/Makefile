CFLAGS :=-O2 -Wall -Wextra -Wunused-function
TARGET := fuzzer

.PHONY: all clean

all: $(TARGET)

fuzzer: fuzzer.c
	$(CC) $(CFLAGS) -I ${KRN_SOURCES}/arch/x86/include $< -o $@ -lpthread

clean:
	make -C $(KRN_SOURCES) M=$(PWD) clean
	rm -f fuzzer
