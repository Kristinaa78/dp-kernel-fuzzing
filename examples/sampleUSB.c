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
#define USB_RAW_IOCTL_INIT			_IOW('U', 0, struct usb_init)
#define USB_RAW_IOCTL_RUN			_IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH	_IOR('U', 2, struct usb_event)
#define USB_RAW_IOCTL_EPS_INFO		_IOR('U', 11, struct usb_eps_info)
// - EP-related constants
#define USB_RAW_EP_ADDR_ANY	0xff
#define USB_RAW_EPS_NUM_MAX	30
#define USB_RAW_EP_NAME_MAX	16

// custom endpoint constants
#define EP_NUM_INT_IN			0x0
#define EP_MAX_PACKET_INT		8
#define EP0_MAX_DATA 			256
#define EP_MAX_PACKET_CONTROL	64
#define EP_MAX_PACKET_INT		8

// USB device descriptor-related constants  [https://www.beyondlogic.org/usbnutshell/usb5.shtml]
// - highest version of the USB specification the device supports
#define BCD_USB		0x0200
// - vendor and product ID are used to find a driver
// - vendor ID assigned by the USB-IF
// [https://the-sz.com/products/usbid/index.php]
#define USB_VENDOR	0x046d	// Logitech Inc.
#define USB_PRODUCT	0xc312 	// DeLuxe 250 Keyboard
// (optional) indeces of string descriptors 
// - provide details about manufacturer, product, a serial number
#define STRING_ID_MANUFACTURER	0
#define STRING_ID_PRODUCT	1
#define STRING_ID_SERIAL	2
#define STRING_ID_CONFIG	3
#define STRING_ID_INTERFACE	4

// for HID - Human Interface Devices 
// https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L725
struct hid_class_descriptor {
	__u8  	bDescriptorType;
	__le16 	wDescriptorLength;
} __attribute__ ((packed));

// https://elixir.bootlin.com/linux/latest/source/include/linux/hid.h#L730
struct hid_descriptor {
	__u8  	bLength;
	__u8 	bDescriptorType;
	__le16 	bcdHID;
	__u8  	bCountryCode;
	__u8  	bNumDescriptors;

	struct hid_class_descriptor desc[1];
} __attribute__ ((packed));

// https://github.com/xairy/raw-gadget/blob/master/raw_gadget/raw_gadget.h#L31
struct usb_init {
	__u8	driver_name[UDC_NAME_LENGTH_MAX];
	__u8	device_name[UDC_NAME_LENGTH_MAX];
	__u8	speed;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L70
struct usb_event {
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
	struct usb_event 		inner_event;
	struct usb_ctrlrequest 	ctrl;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L119
// - exposes capabilities based on [struct usb_ep_caps]:
// https://elixir.bootlin.com/linux/latest/source/include/linux/usb/gadget.h#L165
// - endpoint's supported transfer type and direction 
struct usb_ep_caps {
	__u32	type_control: 1;
	__u32	type_iso: 1;
	__u32	type_bulk: 1;
	__u32	type_int: 1;
	__u32	dir_in: 1;
	__u32	dir_out: 1;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L135
// - endpoint's limits (maximum supported packet size, number of streams)
struct usb_ep_limits {
	__u16	maxpacket_limit;
	__u16	max_streams;
	__u32	reserved;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L149
// - info about gadget endpoint
struct usb_ep_info {
	__u8					name[USB_RAW_EP_NAME_MAX];
	__u32					addr;
	struct usb_ep_caps		caps;
	struct usb_ep_limits	limits;
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L160
// - argument for USB_RAW_IOCTL_EPS_INFO ioctl (stores non-control endpoints)
struct usb_eps_info {
	struct usb_ep_info	eps[USB_RAW_EPS_NUM_MAX];
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/raw_gadget.h#L99
// - argument for ioctls (READ/WRITE to EP0)
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

struct usb_device_descriptor usb_device = {
	.bLength =			USB_DT_DEVICE_SIZE,
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =			__constant_cpu_to_le16(BCD_USB),
	.bDeviceClass =		0,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	.bMaxPacketSize0 =	EP_MAX_PACKET_CONTROL,
	.idVendor =			__constant_cpu_to_le16(USB_VENDOR),
	.idProduct =		__constant_cpu_to_le16(USB_PRODUCT),
	.bcdDevice =		0,
	.iManufacturer =	STRING_ID_MANUFACTURER,
	.iProduct =			STRING_ID_PRODUCT,
	.iSerialNumber =	STRING_ID_SERIAL,
	.bNumConfigurations =	1,
};

struct usb_config_descriptor usb_config = {
	.bLength =			USB_DT_CONFIG_SIZE,
	.bDescriptorType =	USB_DT_CONFIG,
	.wTotalLength =		0,  // computed later
	.bNumInterfaces =	1,
	.bConfigurationValue =	1,
	.iConfiguration = 	STRING_ID_CONFIG,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		0x32,
};

struct usb_interface_descriptor usb_interface = {
	.bLength =				USB_DT_INTERFACE_SIZE,
	.bDescriptorType =		USB_DT_INTERFACE,
	.bInterfaceNumber =		0,
	.bAlternateSetting = 	0,
	.bNumEndpoints =		1,
	.bInterfaceClass =		USB_CLASS_HID,
	.bInterfaceSubClass =	1,
	.bInterfaceProtocol =	1,
	.iInterface =			STRING_ID_INTERFACE,
};

char usb_hid_report[] = {
	0x05, 0x01,                    // Usage Page (Generic Desktop)        0
	0x09, 0x06,                    // Usage (Keyboard)                    2
	0xa1, 0x01,                    // Collection (Application)            4
	0x05, 0x07,                    //  Usage Page (Keyboard)              6
	0x19, 0xe0,                    //  Usage Minimum (224)                8
	0x29, 0xe7,                    //  Usage Maximum (231)                10
	0x15, 0x00,                    //  Logical Minimum (0)                12
	0x25, 0x01,                    //  Logical Maximum (1)                14
	0x75, 0x01,                    //  Report Size (1)                    16
	0x95, 0x08,                    //  Report Count (8)                   18
	0x81, 0x02,                    //  Input (Data,Var,Abs)               20
	0x95, 0x01,                    //  Report Count (1)                   22
	0x75, 0x08,                    //  Report Size (8)                    24
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)               26
	0x95, 0x03,                    //  Report Count (3)                   28
	0x75, 0x01,                    //  Report Size (1)                    30
	0x05, 0x08,                    //  Usage Page (LEDs)                  32
	0x19, 0x01,                    //  Usage Minimum (1)                  34
	0x29, 0x03,                    //  Usage Maximum (3)                  36
	0x91, 0x02,                    //  Output (Data,Var,Abs)              38
	0x95, 0x05,                    //  Report Count (5)                   40
	0x75, 0x01,                    //  Report Size (1)                    42
	0x91, 0x01,                    //  Output (Cnst,Arr,Abs)              44
	0x95, 0x06,                    //  Report Count (6)                   46
	0x75, 0x08,                    //  Report Size (8)                    48
	0x15, 0x00,                    //  Logical Minimum (0)                50
	0x26, 0xff, 0x00,              //  Logical Maximum (255)              52
	0x05, 0x07,                    //  Usage Page (Keyboard)              55
	0x19, 0x00,                    //  Usage Minimum (0)                  57
	0x2a, 0xff, 0x00,              //  Usage Maximum (255)                59
	0x81, 0x00,                    //  Input (Data,Arr,Abs)               62
	0xc0,                          //  End Collection                      64
};

struct hid_descriptor usb_hid = {
	.bLength =			9,
	.bDescriptorType =	HID_DT_HID,
	.bcdHID =			__constant_cpu_to_le16(0x0110),
	.bCountryCode =		0,
	.bNumDescriptors =	1,
	.desc =				{
		{
			.bDescriptorType =		HID_DT_REPORT,
			.wDescriptorLength =	sizeof(usb_hid_report),
		}
	},
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
	struct usb_init arg;
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

void usb_fetch(int fd, struct usb_event *event) {
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

void event_log(struct usb_event *event) {
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
int usb_raw_eps_info(int fd, struct usb_eps_info *info) {
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
int  assign_ep_address(struct usb_ep_info *info, struct usb_endpoint_descriptor *ep) {
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
	struct usb_eps_info info = { 0 };

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

int build_config(char *data, int length) {
	struct usb_config_descriptor *config = (struct usb_config_descriptor *)data;
	int total_length = 0;

	if (length >= sizeof(usb_config)) {
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
	fprintf(stdout, "[i] config->wTotalLength: %d\n", total_length);

	return total_length;
}

// from [https://www.usbmadesimple.co.uk/ums_4.htm]
// STANDARD requests: all via control transfers to ep0
// - control transfer starts with a SETUP transaction (= 8B)
int setup_request(int fd, struct usb_control_event *event, struct usb_control_io *io) {
	// determine request type
	switch (event->ctrl.bRequestType & USB_TYPE_MASK) {
		case USB_TYPE_STANDARD:
			fprintf(stdout, "\tUSB_TYPE_STANDARD\n");
			// determine STANDARD request type (out of 8)
			switch (event->ctrl.bRequest) {
				// GET DESCRIPTOR: first to receive
				// - host needs to know MAX_PACKET_SIZE
				case USB_REQ_GET_DESCRIPTOR:
					// determine descriptor TYPE (based on wValues)
					// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L235
					switch (event->ctrl.wValue >> 8) {
						// device descriptor
						case USB_DT_DEVICE:
							memcpy(&io->data[0], &usb_device, sizeof(usb_device));
							io->inner_io.length = sizeof(usb_device);
							return 1;
						// configuration descriptor
						case USB_DT_CONFIG:
							io->inner_io.length = build_config(&io->data[0], sizeof(io->data));
							return 1;
						// string descriptor
						case USB_DT_STRING:
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
							fprintf(stderr, "[-] FAILURE: no response\n");
							return 0;
					}
					break;
				// when host queries previously set configuration
				case USB_REQ_GET_CONFIGURATION:
					io->inner_io.length = build_config(&io->data[0], sizeof(io->data));
					return 1;
				// when driver tries to configure the device
				// usually wValue == 1 (selects 1st config)
				// if wValue == 0, device should be deconfigured
				case USB_REQ_SET_CONFIGURATION:
					// TO-DO
					return 0;
				case USB_REQ_SET_INTERFACE:
					io->inner_io.length = 0;
					return 1;
				case USB_REQ_GET_INTERFACE:
					io->data[0] = usb_interface.bAlternateSetting;
					io->inner_io.length = 1;
					return 1;
				default:
					fprintf(stderr, "[-] FAILURE: no response\n");
					return 0;
			}
			break;
		case USB_TYPE_CLASS:
			// TO-DO
			break;
		case USB_TYPE_VENDOR:
			// TO-DO
			break;
		default:
			fprintf(stderr, "[-] FAILURE: no response\n");
			return 0;
	}
	return 0;
}

// major events [https://www.usbmadesimple.co.uk/ums_4.htm]:
// 1.  USB device gets 'plugged in'
// 2.  host signals a USB RESET to the device
// 3.  device responds to default 0x00 address
// 4.  host sends a request to device's bidirectional ep0 to find out its MAX_PACKET_SIZE
// 5.  host might reset device again
// 6.  host sends a Set Address request with a unique address, device assumes new address
// 7.  host now starts enumerating the device (asks for device/configuration/string descriptors)
// 8.  at this point, device is not configured yet and can only reply to STANDARD requests
// (CONTROL transfer)
// 9.  after device answers, host loads a suitable device driver
// 10. device driver selects a configuration (incoming Set Configuration request)
// 11. after configuration is set, device can work as intended (supports STANDARD and other
// device-specific requests)
void usb_loop(int fd) {
	// endless loop
	while (1) {
		// distinguishing the control event
		struct usb_control_event event;
		event.inner_event.type = 0;
		event.inner_event.length = sizeof(struct usb_control_event);
		// fetch event
		usb_fetch(fd, (struct usb_event *)&event);
		event_log((struct usb_event*)&event);

		if (event.inner_event.type == USB_RAW_EVENT_CONNECT) {
			endpoints_info(fd);
			continue;
		}

		if (event.inner_event.type != USB_RAW_EVENT_CONTROL)
			continue;

		// process CONTROL event
		struct usb_control_io io;	
		io.inner_io.ep 		= 0;
		io.inner_io.flags 	= 0;
		io.inner_io.length 	= 0;

		int reply = setup_request(fd, &event, &io);
		fprintf(stdout, "[i] response: %d\n", reply);
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
