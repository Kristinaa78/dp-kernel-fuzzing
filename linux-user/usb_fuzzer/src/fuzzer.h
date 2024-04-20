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


// for HID - Human Interface Devices
// https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L725
struct hid_class_descriptor {
	__u8  	bDescriptorType;
	__le16 	wDescriptorLength;
} __attribute__ ((packed));

// https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L730
// - [https://www.usb.org/sites/default/files/hid1_11.pdf p. 32 (Sec 6.2.1)]
// - identifies lenght and type of subordinate descriptors for a device
struct hid_descriptor {
	__u8  	bLength; // descriptor's size
	__u8 	bDescriptorType;
	__le16 	bcdHID; // HID Class specification version
	__u8  	bCountryCode; // country code of the localized hardware
	// number of class descriptors (at least 1 - Report descriptor)
	__u8  	bNumDescriptors;

	struct hid_class_descriptor desc[1];
} __attribute__ ((packed));

// custom struct
struct custom_descriptors {
	struct usb_device_descriptor    *device;
	struct usb_config_descriptor    *config;
	struct usb_interface_descriptor *interface;
    struct usb_endpoint_descriptor  *endpoint;
    struct hid_descriptor           *hid;
};

// taken from: 
// [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L38]
enum usb_raw_event_type {
	USB_RAW_EVENT_INVALID 	 = 0,
	/* This event is queued when the driver has bound to a UDC. */
	USB_RAW_EVENT_CONNECT 	 = 1,
	/* This event is queued when a new control request arrived to ep0. */
	USB_RAW_EVENT_CONTROL 	 = 2,
	/*
	 * These events are queued when the gadget driver is suspended,
	 * resumed, reset, or disconnected. Note that some UDCs (e.g. dwc2)
	 * report a disconnect event instead of a reset.
	 */
	USB_RAW_EVENT_SUSPEND 	 = 3,
	USB_RAW_EVENT_RESUME 	 = 4,
	USB_RAW_EVENT_RESET 	 = 5,
	USB_RAW_EVENT_DISCONNECT = 6,
};

/*
 * usb_init - passed as an argument to raw gadget initialization ioctl
 * @driver_name: name of the UDC driver
 * @device_name: name of the UDC instance
 *
 * see constants.h:3-5 for how this data can be retrieved
 */
struct usb_init {
	__u8	driver_name[UDC_NAME_LENGTH_MAX];
	__u8	device_name[UDC_NAME_LENGTH_MAX];
	__u8	speed;
};

/*
 * struct usb_event - passed as argument to event fetching ioctl
 * @type - type of event ~ from enum usb_raw_event_type at
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L38]
 * @length: data buffer length - driver sets this field
 * @data: fetched event data
 *
 * if type == USB_RAW_EVENT_CONTROL, data[] contains struct usb_ctrlrequest
 */
struct usb_event {
	__u32		type;
	__u32		length;
	__u8		data[];
};

/*
 * usb_control_event - wrapper for control event
 * @inner_event: fetched usb_event
 * @ctrl: filled only for control events
 */
struct usb_control_event {
	struct usb_event 		inner_event;
	struct usb_ctrlrequest 	ctrl;
};

/*
 * usb_ep_caps - capabilities as in struct usb_ep
 */
struct usb_ep_caps {
	__u32	type_control: 1;
	__u32	type_iso: 1;
	__u32	type_bulk: 1;
	__u32	type_int: 1;
	__u32	dir_in: 1;
	__u32	dir_out: 1;
};

/*
 * usb_ep_limits - limits as in struct usb_ep
 * @maxpacket_limit: maximum packet size that endpoint supports
 * @max_streams: maximum number of streaps that endpoint supports
 * @reserved: as of now, value is always empty
 */
struct usb_ep_limits {
	__u16	maxpacket_limit;
	__u16	max_streams;
	__u32	reserved;
};

/*
 * usb_ep_info - information about a gadget endpoint
 * @name: name of the endpoint as defined by UDC driver
 * @addr: address of the endpoint - needs to be specified when enabling
 * the endpoint with USB_RAW_IOCTL_EP_ENABLE
 * @caps: capabilities of the endpoint
 * @limits: limits of the endpoint
 */
struct usb_ep_info {
	__u8					name[USB_RAW_EP_NAME_MAX];
	__u32					addr;
	struct usb_ep_caps		caps;
	struct usb_ep_limits	limits;
};

/*
 * usb_eps_info - passed to ioctl querying information about UDC's non-control eps
 * @eps: information about non-control endpoints
 */
struct usb_eps_info {
	struct usb_ep_info	eps[USB_RAW_EPS_NUM_MAX];
};

struct usb_ep_io {
	__u16		ep;
	__u16		flags;
	__u32		length;
	__u8		data[];
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
/*
 * usb_open - creates a raw gadget instance by opening /dev/raw-gadget
 */
static int usb_open() {     
	return open(DEV_RAW_GADGET, O_RDWR);	
}

/*
 * usb_init - initializes the raw gadget instance
 * @fd:	file descriptor of open /dev/raw-gadget
 * @speed: int value from usb_device_speed enum as in
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L1179]
 * @driver: name of the UDC driver (['dummy_udc'] in case of a virtually emulated dev.
 * @device: name of the UDC instance (['dummy_udc.0'])
 */
static int usb_init(int fd, int speed, const char* driver, const char* device) {
	struct usb_init arg;
    strncpy((char *)&arg.driver_name, driver, UDC_NAME_LENGTH_MAX);
	strncpy((char *)&arg.device_name, device, UDC_NAME_LENGTH_MAX);
	arg.speed = speed;
	return ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
}

/*
 * usb_run - launches the raw gadget instance
 * @fd:	file descriptor of open /dev/raw-gadget
 *
 * raw gadget binds to a dummy_udc and device emulation starts
 */
static int usb_run(int fd) {
	return ioctl(fd, USB_RAW_IOCTL_RUN, 0);
}

/*
 * usb_fetch - waits for an event and returns fetched data
 * @fd:	file descriptor of open /dev/raw-gadget
 * @event: pointer to the usb_event struct
 *
 * this is a blocking ioctl
 */
static int usb_fetch(int fd, struct usb_event *event) {
	return ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, event);
}

// functions for EP interaction ---------------------------------------------------
/*
 * usb_eps_info - queries information about the UDC's available non-control eps
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_eps_info struct
 */
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

/*
 * usb_assing_address - assigns address to endpoint
 * @info: pointer to usb_ep_info containing information about endpoint
 * @ep: pointer to usb_endpoint_descriptor whose address will get assigned
 *
 * all functions defined in:
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h]
 * - usb_endpoint_num 		- gets endpoint's number (0 - 15)
 * - usb_endpoint_dir_in	- checks if endpoint has IN direction
 * - usb_endpoint_dir_out	- checks if endpoint has OUT direction
 * - usb_endpoint_maxp 		- gets endpoint's maximum supported packet size
 * - usb_endpoint_type 		- gets the endpoint's supported transfer type
 */
static int usb_assign_address(struct usb_ep_info *info, struct usb_endpoint_descriptor *ep) {
	if (usb_endpoint_num(ep) != 0) return 0;
	if (usb_endpoint_dir_in(ep) && !info->caps.dir_in) return 0;
	if (usb_endpoint_dir_out(ep) && !info->caps.dir_out) return 0;
	if (usb_endpoint_maxp(ep) > info->limits.maxpacket_limit) return 0;
	// checks ep's transfer type
	switch (usb_endpoint_type(ep)) {
		case USB_ENDPOINT_XFER_BULK:
			if (!info->caps.type_bulk)
				return 0;
			break;
		case USB_ENDPOINT_XFER_INT:
			if (!info->caps.type_int)
				return 0;
			break;
		default:
			return 0;
	}

	// USB_RAW_EP_ADDR_ANY [0xFF] means that EP accepts ANY address
	if (info->addr == USB_RAW_EP_ADDR_ANY) {
		static int addr = 1;
		ep->bEndpointAddress |= addr++;
	} else
		ep->bEndpointAddress |= info->addr;
	return 1;
}

/*
 * usb_endpoints_info - queries the information about the UDC's endpoints and
 * assigns address to the endpoint passed as argument
 * @fd:	file descriptor of open /dev/raw-gadget
 * @endpoint: pointer to the endpoint descriptor
 */
static void usb_endpoints_info(int fd, struct usb_endpoint_descriptor *endpoint) {
	struct usb_eps_info info = { 0 };
	// query the number of endpoints and further information
	int eps = usb_eps_info(fd, &info);
	for (int i = 0; i < eps; i++)
		if (usb_assign_address(&info.eps[i], endpoint))
			continue;
}

// USB setup functions -----------------------------------------------------------
static int usb_configure(int fd) {
 	return ioctl(fd, USB_RAW_IOCTL_CONFIGURE, 0);
}

static int usb_vbus_draw(int fd, uint32_t power) {
	return ioctl(fd, USB_RAW_IOCTL_VBUS_DRAW, power);
}

static int build_config(char *data, uint32_t length, struct custom_descriptors *descriptors) {
	struct usb_config_descriptor *config = (struct usb_config_descriptor *)data;
	int total_length = 0;

	if (length >= sizeof(*descriptors->config)) {
		memcpy(data, descriptors->config, sizeof(*descriptors->config));
		data += sizeof(*descriptors->config);
		length -= sizeof(*descriptors->config);
		total_length += sizeof(*descriptors->config);
	}

	if (length >= sizeof(*descriptors->interface)) {
  		memcpy(data, descriptors->interface, sizeof(*descriptors->interface));                                                        
  		data += sizeof(*descriptors->interface);
  		length -= sizeof(*descriptors->interface);
  		total_length += sizeof(*descriptors->interface);
	}
	
	if (length >= sizeof(*descriptors->hid)) {     
   		memcpy(data, descriptors->hid, sizeof(*descriptors->hid));
   		data += sizeof(*descriptors->hid);         
	    length -= sizeof(*descriptors->hid);       
        total_length += sizeof(*descriptors->hid);    
    }

	if (length >= USB_DT_ENDPOINT_SIZE) {
		memcpy(data, descriptors->endpoint, USB_DT_ENDPOINT_SIZE);
		data += USB_DT_ENDPOINT_SIZE;
		length -= USB_DT_ENDPOINT_SIZE;
		total_length += USB_DT_ENDPOINT_SIZE;
	}

	config->wTotalLength = __cpu_to_le16(total_length);
	return total_length;
}

static int configure_device(int fd, struct usb_config_descriptor *config) {
	int result = 0;
	result = usb_vbus_draw(fd, config->bMaxPower);
	if (result < 0) return result;
	result = usb_configure(fd);
	if (result < 0) return result;
	return 0;
}
