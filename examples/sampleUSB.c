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

#define DEV_RAW_GADGET 		"/dev/raw-gadget"
#define DEVICE_NAME  		"dummy_udc.0"
#define DRIVER_NAME  		"dummy_udc"
#define UDC_NAME_LENGTH_MAX 128

// from https://github.com/xairy/raw-gadget/blob/master/raw_gadget/raw_gadget.h
// - IOCTLS definition
#define USB_RAW_IOCTL_INIT			_IOW('U', 0, struct usb_init)
#define USB_RAW_IOCTL_RUN			_IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH	_IOR('U', 2, struct usb_event)
// - EP-related IOCTLs
#define USB_RAW_IOCTL_EPS_INFO		_IOR('U', 11, struct usb_eps_info)
#define USB_RAW_IOCTL_EP0_STALL		_IO('U', 12)
#define USB_RAW_IOCTL_EP0_WRITE		_IOW('U', 3, struct usb_ep_io)
#define USB_RAW_IOCTL_EP0_READ		_IOWR('U', 4, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_ENABLE		_IOW('U', 5, struct usb_endpoint_descriptor)
#define USB_RAW_IOCTL_EP_DISABLE	_IOW('U', 6, __u32)
#define USB_RAW_IOCTL_EP_WRITE		_IOW('U', 7, struct usb_ep_io)
#define USB_RAW_IOCTL_EP_READ		_IOWR('U', 8, struct usb_ep_io)
// - CONFIG-related IOCTLs
#define USB_RAW_IOCTL_CONFIGURE		_IO('U', 9)
#define USB_RAW_IOCTL_VBUS_DRAW		_IOW('U', 10, __u32)
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
#define STRING_ID_SERIAL	23
#define STRING_ID_CONFIG	3
#define STRING_ID_INTERFACE	4
// HID-related constants
#define USB_HID_HID		0x21
#define USB_HID_REPORT	0x22
#define USB_HID_PHYS	0x23

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

struct usb_int_io {
	struct usb_ep_io	inner_io;
	char				data[EP_MAX_PACKET_INT];
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L286
// - describes general information about a USB device
// - high-speed device also needs device_qualifier descriptor
struct usb_device_descriptor usb_device = {
	.bLength =			USB_DT_DEVICE_SIZE,	// descriptor's size
	.bDescriptorType =	USB_DT_DEVICE,
	.bcdUSB =			__constant_cpu_to_le16(BCD_USB),
	.bDeviceClass =		0, // class code (if 0, interface specifies its own)
	.bDeviceSubClass =	0, // subclass code (if class code 0, this needs to be 0 as well)
	.bDeviceProtocol =	0, // class-specific protocol code
	.bMaxPacketSize0 =	EP_MAX_PACKET_CONTROL, // max packet size for EP0 [only valid: 8/16/32/64]
	.idVendor =			__constant_cpu_to_le16(USB_VENDOR),  // assigned by USB-IF
	.idProduct =		__constant_cpu_to_le16(USB_PRODUCT), // assigned by manufacturer
	.bcdDevice =		0,
	.iManufacturer =	STRING_ID_MANUFACTURER,
	.iProduct =			STRING_ID_PRODUCT,
	.iSerialNumber =	STRING_ID_SERIAL,
	.bNumConfigurations =	1,
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L346
// - describes information about a specific device configuration
// - device has one or more such descriptors
// - each configuration has one or more interfaces
// - each interface has zero or more endpoints
struct usb_config_descriptor usb_config = {
	.bLength =			USB_DT_CONFIG_SIZE, // descriptor's size
	.bDescriptorType =	USB_DT_CONFIG,
	// total length gets computed later
	// - it is total length of data returned for specified configuration
	// - includes: combined length of ALL descriptors (configuration, interface, endpoint, AND
	// class/vendor-specific)
	.wTotalLength =		0, 
	.bNumInterfaces =	1, 	// number of interfaces
	.bConfigurationValue =	1,	// value causes device to assume the described configuration
	.iConfiguration = 	STRING_ID_CONFIG,
	// bitmap: configuration characteristics
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		0x32, // maximum power consumption of the device in this configuration
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L389
// - describes a specific interface within a configuration
// - has zero or more endpoint descriptors (without EP0)
// - always returned as part of the configuration descriptor (interface desc. cannot be
// directly accessed)
// - SetInterface()/GetInterface() work only with alternative interfaces
struct usb_interface_descriptor usb_interface = {
	.bLength =				USB_DT_INTERFACE_SIZE,	// descriptor's size
	.bDescriptorType =		USB_DT_INTERFACE,
	.bInterfaceNumber =		0, // interface's number: index in the array of supported interfaces
	.bAlternateSetting = 	0, // value specifies alternate setting for the interface
	.bNumEndpoints =		1, // number of endpoints (if 0, interface only uses Default Control Pipe)
	.bInterfaceClass =		USB_CLASS_HID, // class code
	.bInterfaceSubClass =	1, // subclass code
	.bInterfaceProtocol =	1, // subclass/class-specific protocol code
	.iInterface =			STRING_ID_INTERFACE,
};

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L407
// - describes information about the bandwidth requirements of the endpoint
// - always returned as part of the configuration information
// - cannot be directly accessed 
// - EP0 has no descriptor
// - misses 2 fields (bRefresh and bSynchAddress used in audio)
struct usb_endpoint_descriptor usb_endpoint = {
	.bLength =			USB_DT_ENDPOINT_SIZE, // descriptor's size
	.bDescriptorType =	USB_DT_ENDPOINT,
	// encoded address of EP on the USB device
	.bEndpointAddress =	USB_DIR_IN | EP_NUM_INT_IN,
	// bitmap: endpoint's transfer type (with ISOsynchronous transfers + synchro type and usage type added)
	.bmAttributes =		USB_ENDPOINT_XFER_INT,	// interrupt endpoint type
	// maximum packet size this EP is capable of sending/receiving
	.wMaxPacketSize =	EP_MAX_PACKET_INT,
	// interval for polling EP for data transfers
	// - expressed in FRAMES/microFRAMES (depending on the device's speed)
	.bInterval =		5,
};

// https://www.usb.org/sites/default/files/hid1_11.pdf
// - HID report descriptor
// - describes device's capabilities  (https://docs.kernel.org/hid/hidintro.html)
// - hexdump -C /sys/bus/hid/devices/{{ id }}/report_descriptor
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
	0xc0,                          // End Collection                      64
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
			fprintf(stdout, "\t\t\tUSB_TYPE_CLASS\n\t\t\t");
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
			fprintf(stdout, "\t\t\tUSB_TYPE_VENDOR\n\t\t\t");
			break;
		case USB_TYPE_RESERVED:
			fprintf(stdout, "\t\t\tUSB_TYPE_RESERVED\n\t\t\t");
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
	// fprintf(stdout, "    ----------------------------------------------------------------------------\n");
}

void event_log(struct usb_event *event) {
	// fprintf(stdout, "[i] EVENT TYPE: %d, LENGTH: %u\n", event->type, event->length);
	fprintf(stdout, "\n");
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
int usb_eps_info(int fd, struct usb_eps_info *info) {
	int result = ioctl(fd, USB_RAW_IOCTL_EPS_INFO, info);
	
	if (result  < 0)
		fprintf(stderr, "[-] USB_RAW_IOCTL_EPS_INFO failed with %d\n", result);

	return result;
}

int usb_ep0_read(int fd, struct usb_ep_io *io) {
	int result  = ioctl(fd, USB_RAW_IOCTL_EP0_READ, io);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP0_READ failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep0_write(int fd, struct usb_ep_io *io) {
	int result = ioctl(fd, USB_RAW_IOCTL_EP0_WRITE, io);
	
	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP0_WRITE failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep_enable(int fd, struct usb_endpoint_descriptor *desc) {
	int result = ioctl(fd, USB_RAW_IOCTL_EP_ENABLE, desc);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP_ENABLE failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep_disable(int fd, int ep) {
	int result = ioctl(fd, USB_RAW_IOCTL_EP_DISABLE, ep);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP_DISABLE failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep_read(int fd, struct usb_ep_io *io) {
	int result = ioctl(fd, USB_RAW_IOCTL_EP_READ, io);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP_READ failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep_write(int fd, struct usb_ep_io *io) {
	int result  = ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);

	if (result < 0) {
		fprintf(stderr, "[-] USB_RAW_IOCTL_EP_WRITE failed with %d\n", result);
		return 0;
	}

	return result;
}

int usb_ep_write_may_fail(int fd, struct usb_ep_io *io) {
	return ioctl(fd, USB_RAW_IOCTL_EP_WRITE, io);
}

void usb_configure(int fd) {
	int result  = ioctl(fd, USB_RAW_IOCTL_CONFIGURE, 0);

	if (result < 0) fprintf(stderr, "[-] USB_RAW_IOCTL_CONFIGURE failed with %d\n", result);
}

void usb_vbus_draw(int fd, uint32_t power) {
	int result = ioctl(fd, USB_RAW_IOCTL_VBUS_DRAW, power);

	if (result < 0) fprintf(stderr, "[-] USB_RAW_IOCTL_VBUS_DRAW failed with %d\n", result);
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

// halted EP0: host needs 'recovery action' to resume the endpoint
void endpoint_stall(int fd) {
	int result = ioctl(fd, USB_RAW_IOCTL_EP0_STALL, 0);

	if (result < 0) fprintf(stderr, "[-] USB_RAW_IOCTL_EP0_STALL failed with %d\n", result);
	
	fprintf(stdout, "[i] EP0 bump\n");
}

void endpoints_info(int fd) {
	struct usb_eps_info info = { 0 };

	int eps = usb_eps_info(fd, &info); // number of endpoints
	/*
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
	*/
	for (int i = 0; i < eps; i++)
		if (assign_ep_address(&info.eps[i], &usb_endpoint))
			continue;

	int ep_int_in_addr = usb_endpoint_num(&usb_endpoint); // get EP's address
	if (ep_int_in_addr != 0) {
		fprintf(stdout, "[i] EP_INT_IN: ADDR = %u ASSIGNED\n", ep_int_in_addr);
		fprintf(stdout, "----------------------------------------\n");
	}
}

// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L338
// - builds configuration descriptor
// - wTotalLength = total length (#B) including interface and endpoint descriptors
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

int ep_int_in = -1;
pthread_t ep_int_in_thread;
int ep_int_in_thread_spawned = 0;

void *ep_int_in_loop(void *arg) {
	fprintf(stdout, "[i] RUNNING IN LOOP\n");
	int fd = (int)(long)arg;
	struct usb_int_io io;
	io.inner_io.ep = ep_int_in;
	io.inner_io.flags = 0;
	io.inner_io.length = 8;

	while (1) {
		memcpy(&io.inner_io.data[0], "\x00\x00\x1b\x00\x00\x00\x00\x00", 8);
		int result = usb_ep_write_may_fail(fd, (struct usb_ep_io *)&io);

		if (result < 0 && errno == ESHUTDOWN) {
			fprintf(stdout, "[i] ep_int_in: device was likely reset, exiting\n");
			break;
		} else if (result < 0) {
			perror("usb_ep_write_may_fail()");
			return NULL;
		}
		fprintf(stdout, "ep_int_in: key down: %d\n", result);

		memcpy(&io.inner_io.data[0], "\x00\x00\x00\x00\x00\x00\x00\x00", 8);
		result = usb_ep_write_may_fail(fd, (struct usb_ep_io *)&io);
		if (result < 0 && errno == ESHUTDOWN) {
			fprintf(stdout, "[i] ep_int_in: device was likely reset, exiting\n");
			return NULL;
		} else if (result < 0) {
			fprintf(stderr, "[i] usb_ep_write_may_fail()");
			return NULL;
		}
		fprintf(stdout, "[i] ep_int_in: key up: %d\n", result);

		sleep(1);
	}

	return NULL;
}

// from [https://www.usbmadesimple.co.uk/ums_4.htm]
// STANDARD requests: all via control transfers to ep0
// - control transfer starts with a SETUP transaction (= 8B)
int setup_request(int fd, struct usb_control_event *event, struct usb_control_io *io) {
	// determine request type
	switch (event->ctrl.bRequestType & USB_TYPE_MASK) {
		case USB_TYPE_STANDARD:
			fprintf(stdout, "\n    USB_TYPE_STANDARD ⟶  ");
			// determine STANDARD request type (out of 8)
			switch (event->ctrl.bRequest) {
				// GET DESCRIPTOR: first to receive
				// - host needs to know MAX_PACKET_SIZE
				case USB_REQ_GET_DESCRIPTOR:
					fprintf(stdout, "USB_REQ_GET_DESCRIPTOR ⟶  ");
					// determine descriptor TYPE (based on wValues)
					// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L235
					switch (event->ctrl.wValue >> 8) {
						// device descriptor
						case USB_DT_DEVICE:
							fprintf(stdout, "USB_DT_DEVICE\n");
							memcpy(&io->data[0], &usb_device, sizeof(usb_device));
							io->inner_io.length = sizeof(usb_device);
							return 1;
						// configuration descriptor
						case USB_DT_CONFIG:
							fprintf(stdout, "USB_DT_CONFIG\n");
							io->inner_io.length = build_config(&io->data[0], sizeof(io->data));
							return 1;
						// string descriptor
						case USB_DT_STRING:
							fprintf(stdout, "USB_DT_STRING\n");
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
						case HID_DT_REPORT: 
							fprintf(stdout, "HID_DT_REPORT\n");
							memcpy(&io->data[0], &usb_hid_report[0], sizeof(usb_hid_report));
							io->inner_io.length = sizeof(usb_hid_report);
							return 1;
						default:
							fprintf(stderr, "[-] FAILURE: no response\n");
							return 0;
					}
					break;
				// when host queries previously set configuration
				case USB_REQ_GET_CONFIGURATION:
					fprintf(stdout, "USB_REQ_GET_CONFIGURATION\n");
					io->inner_io.length = build_config(&io->data[0], sizeof(io->data));
					return 1;
				// when driver tries to configure the device
				// usually wValue == 1 (selects 1st config)
				// if wValue == 0, device should be deconfigured
				case USB_REQ_SET_CONFIGURATION:
 					fprintf(stdout, "USB_REQ_SET_CONFIGURATION\n");
					ep_int_in = usb_ep_enable(fd, &usb_endpoint);
					fprintf(stdout, "\t ├── EP0: ep_int_in enabled: %d\n", ep_int_in);
					// spawns new thread
					// - invokes ep_int_in_loop as its start_routine() with fd as arg
					// - thread terminates if it calls pthread_exit(),  returns from
					// start_routine(), main thread exits(), or gets canceled
					int result = pthread_create(&ep_int_in_thread, 0, ep_int_in_loop, (void *)(long)fd);
					if (result != 0) {
						fprintf(stderr, "[-] UNABLE TO CRETE THREAD (ep_int_in)");
						return 0;
					}
					ep_int_in_thread_spawned =  1;
					fprintf(stdout, "[+] EP0: SPAWNED THREAD (ep_int_in)\n");
					usb_vbus_draw(fd, usb_config.bMaxPower);
					usb_configure(fd);
					io->inner_io.length = 0;
					// when new HID device (e.g., keyboard) is added, it is firstly managed by 
					// /drivers/hid/hid-core.c --> hid_add_device()
					return 1;
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
			switch (event->ctrl.bRequest) {
				case HID_REQ_SET_REPORT:
					// This is an OUT request, so don't initialize data.
					io->inner_io.length = 1;
					return 1;
				case HID_REQ_SET_IDLE:
					io->inner_io.length = 0;
					return 1;
				case HID_REQ_SET_PROTOCOL:
					io->inner_io.length = 0;
					return 1;
				default:
					fprintf(stderr, "[-] FAILURE: no response\n");
					return 0;
			}
			break;
		case USB_TYPE_VENDOR:
			switch (event->ctrl.bRequest) {
				default:
					fprintf(stderr, "[-] FAILURE: no response\n");
					return 0;
				}
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
		if (!reply) {
			fprintf(stderr, "[-] EP0 HALTED\n");
			endpoint_stall(fd);
			continue;
		}

		if (event.ctrl.wLength < io.inner_io.length)
			io.inner_io.length = event.ctrl.wLength;

		if (event.ctrl.bRequestType & USB_DIR_IN) {
			int rv = usb_ep0_write(fd, (struct usb_ep_io *)&io);
			fprintf(stdout, "[i] EP0: transferred %d bytes (in)\n", rv);
		} else {
			int rv = usb_ep0_read(fd, (struct usb_ep_io *)&io);
			fprintf(stdout, "[i] EP0: transferred %d bytes (out)\n", rv);
		}

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
