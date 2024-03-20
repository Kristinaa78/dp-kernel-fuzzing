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

#include "rg-constants.h"

// IOCTLS definition
#define USB_RAW_IOCTL_INIT			_IOW('U', 0, struct usb_init)
#define USB_RAW_IOCTL_RUN			_IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH	_IOR('U', 2, struct usb_event)
// EP-related IOCTLs
#define USB_RAW_IOCTL_EPS_INFO		_IOR('U', 11, struct usb_eps_info)
#define USB_RAW_IOCTL_EP0_STALL		_IO('U', 12)
#define USB_RAW_IOCTL_EP0_WRITE		_IOW('U', 3, struct usb_ep_io)
#define USB_RAW_IOCTL_EP0_READ		_IOWR('U', 4, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_ENABLE		_IOW('U', 5, struct usb_endpoint_descriptor)
#define USB_RAW_IOCTL_EP_DISABLE	_IOW('U', 6, __u32)
#define USB_RAW_IOCTL_EP_WRITE		_IOW('U', 7, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_READ		_IOWR('U', 8, struct usb_ep_io)
// CONFIG-related IOCTLs
#define USB_RAW_IOCTL_CONFIGURE		_IO('U', 9)
#define USB_RAW_IOCTL_VBUS_DRAW		_IOW('U', 10, __u32)

// RawGadget enums
enum usb_raw_event_type {
	USB_RAW_EVENT_INVALID = 0,
	/* This event is queued when the driver has bound to a UDC. */
	USB_RAW_EVENT_CONNECT = 1,
	/* This event is queued when a new control request arrived to ep0. */
	USB_RAW_EVENT_CONTROL = 2,
	/*
	 * These events are queued when the gadget driver is suspended,
	 * resumed, reset, or disconnected. Note that some UDCs (e.g. dwc2)
	 * report a disconnect event instead of a reset.
	 */
	USB_RAW_EVENT_SUSPEND = 3,
	USB_RAW_EVENT_RESUME = 4,
	USB_RAW_EVENT_RESET = 5,
	USB_RAW_EVENT_DISCONNECT = 6,
};

// structs used in IOCTLS
struct usb_init {
	__u8	driver_name[UDC_NAME_LENGTH_MAX];
	__u8	device_name[UDC_NAME_LENGTH_MAX];
	__u8	speed;
};

struct usb_event {
	__u32		type;
	__u32		length;
	// if type == USB_RAW_EVENT_CONTROL, buffer contains struct usb_ctrlrequest
	__u8		data[USB_MAX_PACKET_SIZE];
};

struct usb_control_event {
	struct usb_event 		inner_event;
	struct usb_ctrlrequest 	ctrl;
};

struct usb_ep_caps {
	__u32	type_control: 1;
	__u32	type_iso: 1;
	__u32	type_bulk: 1;
	__u32	type_int: 1;
	__u32	dir_in: 1;
	__u32	dir_out: 1;
};

struct usb_ep_limits {
	__u16	maxpacket_limit;
	__u16	max_streams;
	__u32	reserved;
};

struct usb_ep_info {
	__u8					name[USB_RAW_EP_NAME_MAX];
	__u32					addr;
	struct usb_ep_caps		caps;
	struct usb_ep_limits	limits;
};

struct usb_eps_info {
	struct usb_ep_info	eps[USB_RAW_EPS_NUM_MAX];
};

struct usb_ep_io {
	__u16		ep;
	__u16		flags;
	__u32		length;
	__u8		data[USB_MAX_PACKET_SIZE];
};

struct usb_control_io {
	struct usb_ep_io	inner_io;
	char				data[EP0_MAX_DATA];
};

struct usb_int_io {
	struct usb_ep_io	inner_io;
	char				data[USB_MAX_PACKET_SIZE];
};

static int usb_open() {     
	return open(DEV_RAW_GADGET, O_RDWR);	
}

static int usb_init(int fd, int speed, const char* driver, const char* device) {
	struct usb_init arg;
	strncpy((char *)&arg.device_name, DEVICE_NAME, UDC_NAME_LENGTH_MAX);
    strncpy((char *)&arg.driver_name, DRIVER_NAME, UDC_NAME_LENGTH_MAX);
	arg.speed = speed;
	return ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
}

static int usb_run(int fd)
{
	return ioctl(fd, USB_RAW_IOCTL_RUN, 0);
}

