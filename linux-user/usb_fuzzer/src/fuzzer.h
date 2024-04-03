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
#include "fuzzing-structs.h"

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

// raw gadget enums
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

// USB initialization functions ---------------------------------------------------
static int usb_open() {     
	return open(DEV_RAW_GADGET, O_RDWR);	
}

static int usb_init(int fd, int speed, const char* driver, const char* device) {
	struct usb_init arg;
	strncpy((char *)&arg.device_name, device, UDC_NAME_LENGTH_MAX);
    strncpy((char *)&arg.driver_name, driver, UDC_NAME_LENGTH_MAX);
	arg.speed = speed;
	return ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
}

static int usb_run(int fd) {
	return ioctl(fd, USB_RAW_IOCTL_RUN, 0);
}

// event fetching functions -------------------------------------------------------
static int usb_fetch(int fd, struct usb_event *event) {
	return ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, event);
}

// functions for EP interaction ---------------------------------------------------
static int usb_eps_info(int fd, struct usb_eps_info *info) {
	return ioctl(fd, USB_RAW_IOCTL_EPS_INFO, info);
}

static int usb_ep0_read(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP0_READ, io);
}

static int usb_ep0_write(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP0_WRITE, io);
}

static int usb_ep_enable(int fd, struct usb_endpoint_descriptor *desc) {
	return ioctl(fd, USB_RAW_IOCTL_EP_ENABLE, desc);
}

static int usb_ep_disable(int fd, int ep) {
	return ioctl(fd, USB_RAW_IOCTL_EP_DISABLE, ep);
}

static int usb_ep_read(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP_READ, io);
}

static int usb_ep_write(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);
}

static int usb_ep_write_may_fail(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);
}

static void usb_endpoint_stall(int fd) {
	ioctl(fd, USB_RAW_IOCTL_EP0_STALL, 0);
}

// USB setup functions -----------------------------------------------------------
static int usb_configure(int fd) {
 	return ioctl(fd, USB_RAW_IOCTL_CONFIGURE, 0);
}

static int usb_vbus_draw(int fd, uint32_t power) {
	return ioctl(fd, USB_RAW_IOCTL_VBUS_DRAW, power);
}

static int set_device_descriptor(int fd, struct usb_control_event *event, struct usb_device *device, 
	struct usb_control_io *io) {
	struct usb_device_descriptor *dev = NULL;
	struct usb_config_descriptor* cfg = NULL;

	// determine request type
	switch (event->ctrl.bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		// determine STANDARD request type (out of 8)
		switch (event->ctrl.bRequest) {
		// GET DESCRIPTOR (first to receive)
		case USB_REQ_GET_DESCRIPTOR:
			// determine descriptor TYPE (based on wValues)
			// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L235
			switch (event->ctrl.wValue >> 8) {
			// device descriptor
			case USB_DT_DEVICE:
				// set up the usb_device_descriptor
				dev = (struct usb_device_descriptor*) device->device;
				memcpy(&io->data[0], dev, sizeof(*dev));
				io->inner_io.length = sizeof(*dev);
				return 1;
			// configuration descriptor
			case USB_DT_CONFIG:
				// usb_device should already have the config
				cfg = (struct usb_config_descriptor *) device->config;
				memcpy(&io->data[0], cfg, sizeof(*cfg));
				io->inner_io.length = sizeof(*cfg);
				return 1;
			// string descriptor
			case USB_DT_STRING:
				// TO-DO - what are these specifically?
				io->data[0] = 4;
				io->data[1] = USB_DT_STRING;
				if ((event->ctrl.wValue & 0xff) == 0) {
					io->data[2] = 0x09;
					io->data[3] = 0x04;
				} else {
					io->data[2] = 'x';
					io->data[3] = 0x00;
				}
				io->inner_io.length = 4;
				return 1;
			default:
				return 0;
			}
			break;
		default:
			return 0;
		}
	}
	return 0;
}

static int build_config(char *data, int length) {
	struct usb_config_descriptor *config = (struct usb_config_descriptor *)data;
	int total_length = 0;

	if (length >= sizeof(struct usb_config_desriptor)) {
		memcpy(data, &usb_config, sizeof(usb_config));
		data += sizeof(usb_config);
		length -= sizeof(usb_config);
		total_length += sizeof(usb_config);
	}

	if (length >= sizeof(usb_interface)) {
		memcpy(data, &usb_interface, sizeof(usb_interface));
		data += sizeof(usb_interface);
		length -= sizeof(usb_interface);
		total_length += sizeof(usb_interface);
	}

	if (length >= sizeof(usb_hid)) {
		memcpy(data, &usb_hid, sizeof(usb_hid));
		data += sizeof(usb_hid);
		length -= sizeof(usb_hid);
		total_length += sizeof(usb_hid);
	}

	if (length >= USB_DT_ENDPOINT_SIZE) {
		memcpy(data, &usb_endpoint, USB_DT_ENDPOINT_SIZE);
		data += USB_DT_ENDPOINT_SIZE;
		length -= USB_DT_ENDPOINT_SIZE;
		total_length += USB_DT_ENDPOINT_SIZE;
	}

	config->wTotalLength = __cpu_to_le16(total_length);
	return total_length;
}



static int configure_device(int fd, struct usb_device *device) {
	int result = 0;
	result = usb_vbus_draw(fd, device->config->bMaxPower);
	if (result < 0) return result;
	result = usb_configure(fd);
	if (result < 0) return result;
	return 0;
}

static int usb_setup_device(int fd, struct usb_device *device) {
	int setup_finished = 0;
	while (!setup_finished) {
		struct usb_control_event event;
		event.inner_event.type = 0;
		event.inner_event.length = sizeof(struct usb_control_event);
		// fetch the event
		int result = usb_fetch(fd, (struct usb_event *)&event);
		if (result < 0) return result;
		// wait for the control event
		if (event.inner_event.type != USB_RAW_EVENT_CONTROL)
			continue;

		// process CONTROL event
		struct usb_control_io io;	
		io.inner_io.ep 		= 0;
		io.inner_io.flags 	= 0;
		io.inner_io.length 	= 0;
		int response = set_device_descriptor(fd, &event, device, &io);
		if (!response) {
			usb_endpoint_stall(fd);
			continue;
		}
		if ((event.ctrl.bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD &&
			 event.ctrl.bRequest == USB_REQ_SET_CONFIGURATION) {
			result = configure_device(fd, device);
			if (result < 0) return result;
		}
		if (event.ctrl.wLength < io.inner_io.length)
			io.inner_io.length = event.ctrl.wLength;
		if (event.ctrl.bRequestType & USB_DIR_IN) {
			int rv = usb_ep0_write(fd, (struct usb_ep_io *)&io);
			fprintf(stderr, "[i] EP0: transferred %d bytes (in)\n", rv);
		} else {
			int rv = usb_ep0_read(fd, (struct usb_ep_io *)&io);
			fprintf(stderr, "[i] EP0: transferred %d bytes (out)\n", rv);
		}
	}
	return 0;
}
