/*
 * author: kristina hrebenarova
 * created: 04-2024
 *
 * this file contains definitions for structures, enums, and functions
 * essential for emulation of a customized USB device with raw gadget
 * interface. the source code of exported raw gadget interface (containing
 * definitions of structures for custom ioctls, enums, and raw gadget IOCTLs):
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h]
 * this file is heavily commented. the provided commentary aims to
 * give reader a necessary and cohesive understanding of both the raw
 * gadget interface and the USB protocol (with focus on the HID class).
 * comments decribing the raw gadget interface are primarily sourced from
 * the previously mentioned raw_gadget.h header file. oftentimes, additional
 * information is provided. other comments are referenced usually right away
 * and the information originates primarily from the USB
 * specification, USB HID 1.1. document, and Linux kernel source code.
 *
 * a significant portion of this code is inspired by the sample raw gadget 
 * code for a HID device from the syzkaller's authors. this code
 * is (licensed under Apache-2.0) is available at
 * [https://github.com/xairy/raw-gadget/blob/master/examples/keyboard.c]
 */

#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>
#include "constants.h"

// RAW GADGET IOCTLS definitions
#define USB_RAW_IOCTL_INIT _IOW('U', 0, struct usb_init)
#define USB_RAW_IOCTL_RUN _IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH _IOR('U', 2, struct usb_event)
// - EP-related IOCTLs
#define USB_RAW_IOCTL_EPS_INFO _IOR('U', 11, struct usb_eps_info)
#define USB_RAW_IOCTL_EP0_STALL _IO('U', 12)
#define USB_RAW_IOCTL_EP0_WRITE _IOW('U', 3, struct usb_ep_io)
#define USB_RAW_IOCTL_EP0_READ _IOWR('U', 4, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_ENABLE _IOW('U', 5, struct usb_endpoint_descriptor)
#define USB_RAW_IOCTL_EP_DISABLE _IOW('U', 6, __u32)
#define USB_RAW_IOCTL_EP_WRITE _IOW('U', 7, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_READ _IOWR('U', 8, struct usb_ep_io)
// - CONFIG-related IOCTLs
#define USB_RAW_IOCTL_CONFIGURE _IO('U', 9)
#define USB_RAW_IOCTL_VBUS_DRAW _IOW('U', 10, __u32)

// for Human Interface Devices, class-specific
// [https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L725]
struct hid_class_descriptor {
	__u8 bDescriptorType;
	__le16 wDescriptorLength;
} __attribute__((packed));

// - [https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L730]
// - [https://www.usb.org/sites/default/files/hid1_11.pdf p. 32 (Sec 6.2.1)]
// - identifies length and type of subordinate descriptors for a device
struct hid_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__le16 bcdHID;
	__u8 bCountryCode;
	__u8 bNumDescriptors;
	struct hid_class_descriptor desc[1];
} __attribute__((packed));

// custom struct for all used descriptors so they can be dereferenced easily
// - as the fields suggests, implementation is limited to only 1 configuration
//   with 1 interface, 1 endpoint, and a single hid descriptor
struct custom_descriptors {
	struct usb_device_descriptor *device;
	struct usb_config_descriptor *config;
	struct usb_interface_descriptor *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct hid_descriptor *hid;
};

// taken directly from:
// [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L38]
enum usb_raw_event_type {
	USB_RAW_EVENT_INVALID = 0,
	USB_RAW_EVENT_CONNECT = 1,
	USB_RAW_EVENT_CONTROL = 2,
	USB_RAW_EVENT_SUSPEND = 3,
	USB_RAW_EVENT_RESUME = 4,
	USB_RAW_EVENT_RESET = 5,
	USB_RAW_EVENT_DISCONNECT = 6,
};

// RAW GADGET IOCTL ARGUMENT DEFINITIONS ---------------------------

/*
 * usb_init - passed as an argument to raw gadget initialization ioctl
 * @driver_name: name of the UDC driver
 * @device_name: name of the UDC instance
 *
 * see constants.h:3-5 for how this data can be retrieved
 */
struct usb_init {
	__u8 driver_name[UDC_NAME_LENGTH_MAX];
	__u8 device_name[UDC_NAME_LENGTH_MAX];
	__u8 speed;
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
	__u32 type;
	__u32 length;
	__u8 data[];
};

/*
 * usb_control_event - wrapper for control event
 * @inner_event: fetched usb_event
 * @ctrl: filled only for control events
 */
struct usb_control_event {
	struct usb_event inner_event;
	struct usb_ctrlrequest ctrl;
};

/*
 * usb_ep_caps - capabilities as in struct usb_ep (device-side
 * representation of an endpoint)
 * [https://elixir.bootlin.com/linux/latest/source/include/linux/usb/gadget.h#L165]
 * @type_control: EP is of a control type (reserved for EP0)
 * @type_iso: EP is of an isochronous transfer type
 * @type_bulk: EP is of a bulk transfer type
 * @type_int: EP is of an interrupt transfer type
 * @dir_in: EP supports IN direction
 * @dir_out: EP supports OUT direction
 */
struct usb_ep_caps {
	__u32 type_control : 1;
	__u32 type_iso : 1;
	__u32 type_bulk : 1;
	__u32 type_int : 1;
	__u32 dir_in : 1;
	__u32 dir_out : 1;
};

/*
 * usb_ep_limits - limits as in struct usb_ep (device-side
 * representation of an endpoint)
 * [https://elixir.bootlin.com/linux/latest/source/include/linux/usb/gadget.h#L225]
 * @maxpacket_limit: maximum packet size that endpoint supports
 * @max_streams: maximum number of streaps that endpoint supports
 * @reserved: as of now, value is always empty
 */
struct usb_ep_limits {
	__u16 maxpacket_limit;
	__u16 max_streams;
	__u32 reserved;
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
	__u8 name[USB_RAW_EP_NAME_MAX];
	__u32 addr;
	struct usb_ep_caps caps;
	struct usb_ep_limits limits;
};

/*
 * usb_eps_info - passed to ioctl querying information about UDC's non-control eps
 * @eps: information about non-control endpoints
 */
struct usb_eps_info {
	struct usb_ep_info eps[USB_RAW_EPS_NUM_MAX];
};

/*
 * usb_ep_io - argument for endpoint read and write ioctls
 * @ep: handle for endpoint which is returned by the ep enable ioctl
 * @flags: request flags
 * @length: length of the payload
 * @data: payload to send/receive data from the kernel
 */
struct usb_ep_io {
	__u16 ep;
	__u16 flags;
	__u32 length;
	__u8 data[];
};

/*
 * usb_control_io - wrapper for control endpoint0 read and write ioctls
 * @inner_io: regular usb_ep_io struct
 * @data: data to write/read from ep0
 */
struct usb_control_io {
	struct usb_ep_io inner_io;
	char data[EP0_MAX_DATA];
};

/*
 * usb_int_io - wrapper for interrupt endpoint read and write ioctls
 * @inner_io: regular usb_ep_io struct
 * @data: data to write/read from the ep
 */
struct usb_int_io {
	struct usb_ep_io inner_io;
	char data[USB_MAX_PACKET_SIZE];
};

/*
 * usb_open - creates a raw gadget instance by opening /dev/raw-gadget
 * returns: file descriptor to open /dev/raw-gadget file
 */
static int usb_open()
{
	return open(DEV_RAW_GADGET, O_RDWR);
}

/*
 * usb_init - initializes the raw gadget instance
 * @fd:	file descriptor of open /dev/raw-gadget
 * @speed: int value from usb_device_speed enum as in
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L1179]
 * @driver: name of the UDC driver (['dummy_udc'] in case of a virtually emulated dev.
 * @device: name of the UDC instance (['dummy_udc.0'])
 * returns: ioctl return value
 */
static int usb_init(int fd, int speed, const char *driver, const char *device)
{
	struct usb_init arg;
	strncpy((char *)&arg.driver_name, driver, UDC_NAME_LENGTH_MAX);
	strncpy((char *)&arg.device_name, device, UDC_NAME_LENGTH_MAX);
	arg.speed = speed;
	return ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
}

/*
 * usb_run - launches the raw gadget instance
 * @fd:	file descriptor of open /dev/raw-gadget
 * returns: ioctl return value
 *
 * raw gadget binds to a dummy_udc and device emulation starts
 */
static int usb_run(int fd)
{
	return ioctl(fd, USB_RAW_IOCTL_RUN, 0);
}

/*
 * usb_fetch - waits for an event and returns fetched data
 * @fd:	file descriptor of open /dev/raw-gadget
 * @event: pointer to the usb_event struct
 * returns: ioctl return value
 *
 * this is a blocking ioctl
 */
static int usb_fetch(int fd, struct usb_event *event)
{
	return ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, event);
}

/*
 * usb_eps_info - queries information about the UDC's available non-control eps
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_eps_info struct
 * returns: ioctl return value (in this case, it is the number of
 * available endpoints)
 */
static int usb_eps_info(int fd, struct usb_eps_info *info)
{
	return ioctl(fd, USB_RAW_IOCTL_EPS_INFO, info);
}

/*
 * usb_ep0_read - sends OUT request as a response to kernel standard
 * control requests (received on EP0)
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_ep_io struct
 * returns: ioctl return value (in this case, it is the length of
 * the transferred data)
 */
static int usb_ep0_read(int fd, struct usb_ep_io *io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP0_READ, io);
}

/*
 * usb_ep0_write - sends IN requests as a response to kernel standard
 * control requests (received on EP0)
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_ep_io struct
 * returns: ioctl return value (in this case, it is the length of
 * the transferred data)
 */
static int usb_ep0_write(int fd, struct usb_ep_io *io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP0_WRITE, io);
}

/*
 * usb_ep_enable - enables the specified endpoint
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_endpoint_descriptor struct
 * returns: ioctl return value (in this case, it is a handle to
 * an enabled endpoint)
 */
static int usb_ep_enable(int fd, struct usb_endpoint_descriptor *desc)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_ENABLE, desc);
}

/*
 * usb_ep_write - sends IN requests as a response to kernel last
 * request. 
 * @fd:	file descriptor of open /dev/raw-gadget
 * @info: pointer to the usb_ep_io struct
 * returns: ioctl return value (in this case, it is the length of
 * the transferred data)
 *
 * function waits until the request is completed
 */
static int usb_ep_write(int fd, struct usb_ep_io *io)
{
	return ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);
}

/*
 * usb_ep_stall - stalls a pending control request on EP0
 * @fd:	file descriptor of open /dev/raw-gadget
 * returns: ioctl return value
 */
static void usb_ep_stall(int fd)
{
	ioctl(fd, USB_RAW_IOCTL_EP0_STALL, 0);
}

/*
 * usb_assign_address - assigns address to endpoint
 * @info: pointer to usb_ep_info containing information about endpoint
 * @ep: pointer to usb_endpoint_descriptor whose address will get assigned
 * returns: 1 on success
 *
 * all functions defined in:
 * [https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h]
 * - usb_endpoint_num 		- gets endpoint's number (0 - 15)
 * - usb_endpoint_dir_in	- checks if endpoint has IN direction
 * - usb_endpoint_dir_out	- checks if endpoint has OUT direction
 * - usb_endpoint_maxp 		- gets endpoint's maximum supported packet size
 * - usb_endpoint_type 		- gets the endpoint's supported transfer type
 */
static int usb_assign_address(struct usb_ep_info *info, struct usb_endpoint_descriptor *ep)
{
	if (usb_endpoint_num(ep) != 0)
		return 0;
	if (usb_endpoint_dir_in(ep) && !info->caps.dir_in)
		return 0;
	if (usb_endpoint_dir_out(ep) && !info->caps.dir_out)
		return 0;
	if (usb_endpoint_maxp(ep) > info->limits.maxpacket_limit)
		return 0;
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
static void usb_endpoints_info(int fd, struct usb_endpoint_descriptor *endpoint)
{
	struct usb_eps_info info = { 0 };
	// query the number of endpoints and further information
	int eps = usb_eps_info(fd, &info);
	for (int i = 0; i < eps; i++)
		if (usb_assign_address(&info.eps[i], endpoint))
			continue;
}

/*
 * usb_configure - sends gadget to the configured state
 * @fd:	file descriptor of open /dev/raw-gadget
 * returns: ioctl return value
 */
static int usb_configure(int fd)
{
	return ioctl(fd, USB_RAW_IOCTL_CONFIGURE, 0);
}

/*
 * usb_vbus_draw - sets constraints on the power usage of the device 
 * @fd:	file descriptor of open /dev/raw-gadget
 * @power: current limit
 * returns: ioctl return value
 */
static int usb_vbus_draw(int fd, uint32_t power)
{
	return ioctl(fd, USB_RAW_IOCTL_VBUS_DRAW, power);
}

/*
 * build_config - builds the configuration descriptor (which contains all other
 * descriptors -- interface, endpoint, and hid descriptor)
 * @data: configuration descriptor to be built
 * @length: descriptor length
 * @descriptors: pointer to custom_descriptors structure for easy dereferencing
 * returns: total calculated length of the configuration
 *
 * the order is prescribed in USB HID 1.1 specification: page 48
 */
static int build_config(char *data, uint32_t length, struct custom_descriptors *descriptors)
{
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

/*
 * configure_device - composite function to set gadget's configuration and
 * the power constraints
 * @fd: file descriptor of open /dev/raw-gadget
 * @config: pointer to usb_config_descriptor
 * returns: 0 on success
 */
static int configure_device(int fd, struct usb_config_descriptor *config)
{
	int result = 0;
	result = usb_vbus_draw(fd, config->bMaxPower);
	if (result < 0)
		return result;
	result = usb_configure(fd);
	if (result < 0)
		return result;
	return 0;
}
