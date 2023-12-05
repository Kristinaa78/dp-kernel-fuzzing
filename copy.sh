#!/bin/bash

sudo mount -t ext4 ${RAW_GADGET_DIR}/buildroot/output/images/rootfs.ext2 /media/dp
# copy data to vm's fs
sudo cp -r ${RAW_GADGET_DIR}/host-data /media/dp
# copy lkm.ko
sudo cp ${RAW_GADGET_DIR}/linux/drivers/dp-lkm/dp-lkm.ko /media/dp
sudo umount /media/dp
