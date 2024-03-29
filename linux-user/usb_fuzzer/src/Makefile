CFLAGS :=-O2 -Wall -Wextra
TARGET := sampleUSB

.PHONY: all clean

all: $(TARGET)

sampleUSB: sampleUSB.c
	$(CC) $(CFLAGS) -I ${KRN_SOURCES}/arch/x86/include $< -o $@ -lpthread

clean:
	make -C $(KRN_SOURCES) M=$(PWD) clean
	rm -f sampleUSB
