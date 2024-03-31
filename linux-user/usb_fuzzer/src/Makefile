CFLAGS :=-O2 -Wall -Wextra -Wunused-function
TARGET := sampleUSB fuzzer

.PHONY: all clean

all: $(TARGET)

sampleUSB: sampleUSB.c
	$(CC) $(CFLAGS) -I ${KRN_SOURCES}/arch/x86/include $< -o $@ -lpthread

fuzzer: fuzzer.c
	$(CC) $(CFLAGS) -I ${KRN_SOURCES}/arch/x86/include $< -o $@ -lpthread

clean:
	make -C $(KRN_SOURCES) M=$(PWD) clean
	rm -f sampleUSB fuzzer
