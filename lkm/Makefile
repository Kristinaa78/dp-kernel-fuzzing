obj-m = dp-lkm.o

all:
	make -C $(LINUX_SOURCE) M=$(PWD) modules

clean:
	make -C $(LINUX_SOURCE) M=$(PWD) clean
