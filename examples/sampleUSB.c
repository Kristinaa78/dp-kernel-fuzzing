#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>

#define DEV_RAW_GADGET 		"/dev/raw-gadget"
#define DEVICE_NAME  		"dummy_udc.0"
#define DRIVER_NAME  		"dummy_udc"
#define UDC_NAME_LENGTH_MAX 128

// from https://github.com/xairy/raw-gadget/blob/master/raw_gadget/raw_gadget.h
// - IOCTLS definition
#define USB_RAW_IOCTL_INIT		_IOW('U', 0, struct usb_raw_init)
#define USB_RAW_IOCTL_RUN		_IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH	_IOR('U', 2, struct usb_raw_event)
#define USB_RAW_IOCTL_EPS_INFO		_IOR('U', 11, struct usb_raw_eps_info)
// - EP-related constants
#define USB_RAW_EP_ADDR_ANY	0xff
#define USB_RAW_EPS_NUM_MAX	30
#define USB_RAW_EP_NAME_MAX	16

// custom endpoint constants
#define EP_NUM_INT_IN		0x0
#define EP_MAX_PACKET_INT	8
#define EP0_MAX_DATA 		256

// https://github.com/xairy/raw-gadget/blob/master/raw_gadget/raw_gadget.h#L31
struct usb_raw_init {
	__u8	driver_name[UDC_NAME_LENGTH_MAX];
	__u8	device_name[UDC_NAME_LENGTH_MAX];
	__u8	speed;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L70
struct usb_raw_event {
	__u32		type;
	__u32		length;
	// if type == USB_RAW_EVENT_CONTROL, buffer contains struct usb_ctrlrequest
	__u8		data[];
};

// wrapper for control event: struct usb_ctrlrequest is at
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L210
// - contains SETUP data for a USB device control request
// - matches fields of the USB 2.0 Spec
// - can be made any time 
struct usb_control_event {
	struct usb_raw_event 	inner_event;
	struct usb_ctrlrequest 	ctrl;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L119
// - exposes capabilities based on [struct usb_ep_caps]:
// https://elixir.bootlin.com/linux/latest/source/include/linux/usb/gadget.h#L165
// - endpoint's supported transfer type and direction 
struct usb_raw_ep_caps {
	__u32	type_control: 1;
	__u32	type_iso: 1;
	__u32	type_bulk: 1;
	__u32	type_int: 1;
	__u32	dir_in: 1;
	__u32	dir_out: 1;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L135
// - endpoint's limits (maximum supported packet size, number of streams)
struct usb_raw_ep_limits {
	__u16	maxpacket_limit;
	__u16	max_streams;
	__u32	reserved;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L149
// - info about gadget endpoint
struct usb_raw_ep_info {
	__u8						name[USB_RAW_EP_NAME_MAX];
	__u32						addr;
	struct usb_raw_ep_caps		caps;
	struct usb_raw_ep_limits	limits;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L160
// - argument for USB_RAW_IOCTL_EPS_INFO ioctl (stores non-control endpoints)
struct usb_raw_eps_info {
	struct usb_raw_ep_info	eps[USB_RAW_EPS_NUM_MAX];
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L99
// - argument for ioctls (READ/WRITE to EP0)
struct usb_raw_ep_io {
	__u16		ep;
	__u16		flags;
	__u32		length;
	__u8		data[];
};

struct usb_raw_control_io {
	struct usb_raw_ep_io	inner;
	char					data[EP0_MAX_DATA];
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L407
// - endpoint descriptor
// - misses 2 fields (bRefresh and bSynchAddress used in audio)
// - constants are from usb/ch9 from USB 2.0 specification
struct usb_endpoint_descriptor usb_endpoint = {
	.bLength =			USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN | EP_NUM_INT_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,	// interrupt endpoint type
	.wMaxPacketSize =	EP_MAX_PACKET_INT,
	.bInterval =		5,
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L38
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


int usb_open() {
	int fd; 

	fprintf(stdout, "[i] opening %s\n", DEV_RAW_GADGET);
	fd = open(DEV_RAW_GADGET, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "[-] %s could not be accessed\n", DEV_RAW_GADGET);
		return -EBADF;
	}

	fprintf(stdout, "[+] %s successfully opened\n", DEV_RAW_GADGET);
	return fd;
}


int usb_init(int fd) {
	struct usb_raw_init arg;
	int result = 0;

	// prepare ioctl arguments
	fprintf(stdout, "[i] preparing USB_RAW_IOCLT_INIT arguments: \n");
	strncpy((char *)&arg.device_name, DEVICE_NAME, UDC_NAME_LENGTH_MAX);
	strncpy((char *)&arg.driver_name, DRIVER_NAME, UDC_NAME_LENGTH_MAX);
	// according to:
	// https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/ch9.h#L1179C5-L1179C5
	arg.speed = USB_SPEED_HIGH; // usb 2.0

	fprintf(stdout, "\tdevice_name: \t%s\n", arg.device_name);
	fprintf(stdout, "\tdriver_name: \t%s\n", arg.driver_name);
	fprintf(stdout, "\tspeed: \t\t%d\n", arg.speed);
	result = ioctl(fd, USB_RAW_IOCTL_INIT, &arg);
	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_INIT failed with %d\n", result);
		return result;
	}

	fprintf(stdout, "[+] instance successfully initialized\n");
	return 0;
}


int usb_run(int fd) {
	int result = ioctl(fd, USB_RAW_IOCTL_RUN, 0);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_RUN failed with %d\n", result);
		return result;		
	}

	fprintf(stdout, "[+] running instance...\n");
	return 0;
}

void usb_fetch(int fd, struct usb_raw_event *event) {
	int result = ioctl(fd, USB_RAW_IOCTL_EVENT_FETCH, event);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EVENT_FETCH failed with %d\n", result);
	}
}

// constants taken from
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L39
void ctrl_log(struct usb_ctrlrequest *event) {
	fprintf(stdout, "[i] CONTROL REQUEST DATA:\n");
	fprintf(stdout, "    ----------------------------------------------------------------------------\n");
	// USB directions (1/3 of bRequestType)
	fprintf(stdout, "    bRequestType:\tUSB_DIR_%s\n",
			(event->bRequestType & USB_DIR_IN) ? "IN\t\t\t[to host]" : "OUT\t\t\t[to device]");
	// USB types (2/3 of bRequestType)	
	switch (event->bRequestType & USB_TYPE_MASK) {
		case USB_TYPE_STANDARD:
			fprintf(stdout, "\t\t\tUSB_TYPE_STANDARD\n\t\t\t");
			// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L72
			switch (event->bRequest) {
				case USB_REQ_GET_STATUS:
					fprintf(stdout, "├── USB_REQ_GET_STATUS\n");
					break;
				case USB_REQ_CLEAR_FEATURE:
					fprintf(stdout, "├── USB_REQ_CLEAR_FEATURE\n");
					break;
				case USB_REQ_SET_FEATURE:
					fprintf(stdout, "├── USB_REQ_SET_FEATURE\n");
					break;
				case USB_REQ_SET_ADDRESS:
					fprintf(stdout, "├── USB_REQ_SET_ADDRESS\n");
					break;
				case USB_REQ_GET_DESCRIPTOR:
					// Standard descriptors:
					// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L221
					fprintf(stdout, "├── USB_REQ_GET_DESCRIPTOR");
					switch (event->wValue >> 8) {
						case USB_DT_DEVICE:
							fprintf(stdout, "\t[desc =  USB_DT_DEVICE]\n");
							break;
						case USB_DT_CONFIG:
							fprintf(stdout, "\t[desc =  USB_DT_CONFIG]\n");
							break;
						case USB_DT_STRING:
							fprintf(stdout, "\t[desc =  USB_DT_STRING]\n");
							break;
						case USB_DT_INTERFACE:
							fprintf(stdout, "\t[desc =  USB_DT_INTERFACE]\n");
							break;
						case USB_DT_ENDPOINT:
							fprintf(stdout, "\t[desc =  USB_DT_ENDPOINT]\n");
							break;
						case USB_DT_DEVICE_QUALIFIER:
							fprintf(stdout, "\t[desc =  USB_DT_DEVICE_QUALIFIER]\n");
							break;
						case USB_DT_OTHER_SPEED_CONFIG:
							fprintf(stdout, "\t[desc =  USB_DT_OTHER_SPEED_CONFIG]\n");
							break;
						case USB_DT_INTERFACE_POWER:
							fprintf(stdout, "\t[desc =  USB_DT_INTERFACE_POWER]\n");
							break;
						case USB_DT_OTG:
							fprintf(stdout, "\t[desc =  USB_DT_OTG]\n");
							break;
						case USB_DT_DEBUG:
							fprintf(stdout, "\t[desc =  USB_DT_DEBUG]\n");
							break;
						case USB_DT_INTERFACE_ASSOCIATION:
							fprintf(stdout, "\t[desc = USB_DT_INTERFACE_ASSOCIATION]\n");
							break;
						case USB_DT_SECURITY:
							fprintf(stdout, "\t[desc = USB_DT_SECURITY]\n");
							break;
						case USB_DT_KEY:
							fprintf(stdout, "\t[desc = USB_DT_KEY]\n");
							break;
						case USB_DT_ENCRYPTION_TYPE:
							fprintf(stdout, "\t[desc = USB_DT_ENCRYPTION_TYPE]\n");
							break;
						case USB_DT_BOS:
							fprintf(stdout, "\t[desc = USB_DT_BOS]\n");
							break;
						case USB_DT_DEVICE_CAPABILITY:
							fprintf(stdout, "\t[desc = USB_DT_DEVICE_CAPABILITY]\n");
							break;
						case USB_DT_WIRELESS_ENDPOINT_COMP:
							fprintf(stdout, "\t[desc = USB_DT_WIRELESS_ENDPOINT_COMP]\n");
							break;
						case USB_DT_PIPE_USAGE:
							fprintf(stdout, "\t[desc = USB_DT_PIPE_USAGE]\n");
							break;
						case USB_DT_SS_ENDPOINT_COMP:
							fprintf(stdout, "\t[desc = USB_DT_SS_ENDPOINT_COMP]\n");
							break;
						case HID_DT_HID:
							fprintf(stdout, "\t[desc = HID_DT_HID]\n");
							break;
						case HID_DT_REPORT:
							fprintf(stdout, "\t[desc = HID_DT_REPORT]\n");
							break;
						case HID_DT_PHYSICAL:
							fprintf(stdout, "\t[desc = HID_DT_PHYSICAL]\n");
							break;
						default:
							fprintf(stdout, "\t[desc = UNKNOWN = 0x%x]\n", event->wValue >> 8);
							break;
					}
					break;
				case USB_REQ_SET_DESCRIPTOR:
					fprintf(stdout, "├── USB_REQ_SET_DESCRIPTOR\n");
					break;
				case USB_REQ_GET_CONFIGURATION:
					fprintf(stdout, "├── USB_REQ_GET_CONFIGURATION\n");
					break;
				case USB_REQ_SET_CONFIGURATION:
					fprintf(stdout, "├── USB_REQ_SET_CONFIGURATION\n");
					break;
				case USB_REQ_GET_INTERFACE:
					fprintf(stdout, "├── USB_REQ_GET_INTERFACE\n");
					break;
				case USB_REQ_SET_INTERFACE:
					fprintf(stdout, "├── USB_REQ_SET_INTERFACE\n");
					break;
				case USB_REQ_SYNCH_FRAME:
					fprintf(stdout, "├── USB_REQ_SYNCH_FRAME\n");
					break;
				case USB_REQ_SET_SEL:
					fprintf(stdout, "├── USB_REQ_SET_SEL\n");
					break;
				case USB_REQ_SET_ISOCH_DELAY:
					fprintf(stdout, "├── USB_REQ_SET_ISOCH_DELAY\n");
					break;
				default:
					fprintf(stdout, "├── UNKNOWN USB REQ = %d\n", (int)event->bRequest);
					break;
			}
			break;
		case USB_TYPE_CLASS:
			fprintf(stdout, "\t\t\tUSB_TYPE_CLASS\n");
			switch (event->bRequest) {
				case HID_REQ_GET_REPORT:
					fprintf(stdout, "├── HID_REQ_GET_REPORT\n");
					break;
				case HID_REQ_GET_IDLE:
					fprintf(stdout, "├── HID_REQ_GET_IDLE\n");
					break;
				case HID_REQ_GET_PROTOCOL:
					fprintf(stdout, "├── HID_REQ_GET_PROTOCOL\n");
					break;
				case HID_REQ_SET_REPORT:
					fprintf(stdout, "├── HID_REQ_SET_REPORT\n");
					break;
				case HID_REQ_SET_IDLE:
					fprintf(stdout, "├── HID_REQ_SET_IDLE\n");
					break;
				case HID_REQ_SET_PROTOCOL:
					fprintf(stdout, "├── HID_REQ_SET_PROTOCOL\n");
					break;
				default:
					fprintf(stdout, "├── UNKNOWN REQ = 0x%x\n", event->bRequest);
					break;
			}
			break;
		case USB_TYPE_VENDOR:
			fprintf(stdout, "\t\t\tUSB_TYPE_VENDOR\n");
			break;
		case USB_TYPE_RESERVED:
			fprintf(stdout, "\t\t\tUSB_TYPE_RESERVED\n");
			break;
		default:
			fprintf(stdout, "\t\t\tUNKNOWN TYPE = 0x%x\n", event->bRequestType);
			break;
	}
	// UBS recipients (3/3 of bRequestType)
	switch (event->bRequestType & USB_RECIP_MASK) {
		case USB_RECIP_DEVICE:
			fprintf(stdout, "\t\t\tUSB_RECIP_DEVICE\n");
			break;
		case USB_RECIP_INTERFACE:
			fprintf(stdout, "\t\t\tUSB_RECIP_INTERFACE\n");
			break;
		case USB_RECIP_ENDPOINT:
			fprintf(stdout, "\t\t\tUSB_RECIP_ENDPOINT\n");
			break;
		case USB_RECIP_OTHER:
			fprintf(stdout, "\t\t\tUSB_RECIP_OTHER\n");
			break;
		default:
			fprintf(stdout, "\t\t\tUNKNOWN RECIP  = %d\n", (int)event->bRequestType);
			break;
	}
	fprintf(stdout, "    ----------------------------------------------------------------------------\n");
}

void event_log(struct usb_raw_event *event) {
	fprintf(stdout, "[i] EVENT TYPE: %d, LENGTH: %u\n", event->type, event->length);
	switch (event->type) {
		case USB_RAW_EVENT_INVALID:
			fprintf(stdout, "[i] USB_RAW_EVENT_INVALID fetched\n");
			break;		
		case USB_RAW_EVENT_CONNECT:
			fprintf(stdout, "[i] USB_RAW_EVENT_CONNECT fetched\n");
			break;
		case USB_RAW_EVENT_CONTROL:
			fprintf(stdout, "[i] USB_RAW_EVENT_CONTROL fetched\n");
			ctrl_log((struct usb_ctrlrequest *)event->data);
			break;
		case USB_RAW_EVENT_SUSPEND:
			fprintf(stdout, "[i] USB_RAW_EVENT_SUSPEND fetched\n");
			break;
		case USB_RAW_EVENT_RESUME:
			fprintf(stdout, "[i] USB_RAW_EVENT_RESUME fetched\n");
			break;
		case USB_RAW_EVENT_RESET:
			fprintf(stdout, "[i] USB_RAW_EVENT_RESET fetched\n");
			break;
		case USB_RAW_EVENT_DISCONNECT:
			fprintf(stdout, "[i] USB_RAW_EVENT_DISCONNECT fetched\n");
			break;
		default:
			fprintf(stdout, "[!] UNKNOWN EVENT TYPE fetched\n");
			break;
	}
}


// queries information about non-control endpoints for current UDC
int usb_raw_eps_info(int fd, struct usb_raw_eps_info *info) {
	int result = ioctl(fd, USB_RAW_IOCTL_EPS_INFO, info);
	
	if (result  < 0)
		fprintf(stderr, "[-] USB_RAW_IOCTL_EPS_INFO failed with %d\n", result);

	return result;
}

// all functions defined in:
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h
// usb_endpoint_num - gets endpoint's number (0 - 15)
// usb_endpoint_dir_in - checks if endpoint has IN direction
// usb_endpoint_dir_out - checks if endpoint has OUT direction
// usb_endpoint_maxp - gets endpoint's maximum supported packet size
// usb_endpoint_type - gets the endpoint's supported transfer type
int  assign_ep_address(struct usb_raw_ep_info *info, struct usb_endpoint_descriptor *ep) {
	if (usb_endpoint_num(ep) != 0) return 0;
	if (usb_endpoint_dir_in(ep) && !info->caps.dir_in) return 0;
	if (usb_endpoint_dir_out(ep) && !info->caps.dir_out) return 0;
	if (usb_endpoint_maxp(ep) > info->limits.maxpacket_limit) return 0;
	// checks bmAttributes of the EP descriptor
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

void endpoints_info(int fd) {
	struct usb_raw_eps_info info = { 0 };

	int eps = usb_raw_eps_info(fd, &info); // number of endpoints
	for (int i = 0; i < eps; i++) {
		fprintf(stdout, "ENDPOINT #%d:\n", i);
		fprintf(stdout, "├── NAME:\t\t%s\n", &info.eps[i].name[0]);
		fprintf(stdout, "├── ADDR:\t\t%u\n", info.eps[i].addr);
		// endpoint's transfer type
		fprintf(stdout, "├── TYPE:\t\t%s %s %s\n",
			info.eps[i].caps.type_iso  ? "ISO" : "___",
			info.eps[i].caps.type_bulk ? "BLK" : "___",
			info.eps[i].caps.type_int  ? "INT" : "___");
		// endpoint's direction (IN or OUT)
		fprintf(stdout, "├── DIR:\t\t%s %s\n",
			info.eps[i].caps.dir_in  ? "IN" : "___",
			info.eps[i].caps.dir_out ? "OUT" : "___");
		// maximum supported packet size
		fprintf(stdout, "├── MAX PACKET LIMIT:\t%u\n",
			info.eps[i].limits.maxpacket_limit);
		// maximum number of streams
		fprintf(stdout, "├── MAX STREAMS:\t%u\n", info.eps[i].limits.max_streams);
		fprintf(stdout, "----------------------------------------\n");
	}

	for (int i = 0; i < eps; i++)
		if (assign_ep_address(&info.eps[i], &usb_endpoint))
			continue;

	int ep_int_in_addr = usb_endpoint_num(&usb_endpoint); // get EP's address
	if (ep_int_in_addr != 0) {
		fprintf(stdout, "[i] EP_INT_IN: ADDR = %u ASSIGNED\n", ep_int_in_addr);
		fprintf(stdout, "----------------------------------------\n");
	}
}

void usb_loop(int fd) {
	// endless loop
	while (1) {
		// distinguishing the control event
		struct usb_control_event event;
		event.inner_event.type = 0;
		event.inner_event.length = sizeof(struct usb_control_event);
		// fetch event
		usb_fetch(fd, (struct usb_raw_event *)&event);
		event_log((struct usb_raw_event*)&event);

		if (event.inner_event.type == USB_RAW_EVENT_CONNECT) {
			endpoints_info(fd);
			continue;
		}

		if (event.inner_event.type != USB_RAW_EVENT_CONTROL)
			continue;


	}
}

int main()
{
	int fd;
	// 1. create a raw gadget instance by opening /dev/raw-gadget
	fd = usb_open();	
	// 2. initialize the instance via USB_RAW_IOCTL_INIT
	usb_init(fd);
	// 3. run the instance with USB_RAW_IOCTL_RUN
	usb_run(fd);
	// 4. in a loop issue USB_RAW_IOCTL_EVENT_FETCH to receive events
	// from Raw Gadget and react to those depending on what kind of USB
	// gadget must be implemented
	usb_loop(fd);
    
	return 0;
}
