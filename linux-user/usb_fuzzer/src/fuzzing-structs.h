#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>

#include "constants.h"

struct usb_endpoint {
    struct usb_endpoint_descriptor ep;
};

struct usb_interface {
    struct usb_interface_descriptor *iface;
    struct usb_endpoint eps[MAX_EPS_NUMBER];
    int eps_num; 
};

struct usb_device {
	struct usb_device_descriptor* device;
	struct usb_config_descriptor* config;
	struct usb_interface ifaces[MAX_INTERFACE_NUMBER];
	int ifaces_num;
	int iface_cur;
};

