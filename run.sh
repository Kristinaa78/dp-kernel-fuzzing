#!/bin/bash
./copy.sh

qemu-system-x86_64 -kernel linux/arch/x86/boot/bzImage \
-serial mon:stdio  \
-boot c -m 512 -drive file=buildroot/output/images/rootfs.ext4,format=raw,index=0,media=disk  \
-display none  -append "root=/dev/sda rw console=ttyS0" \
