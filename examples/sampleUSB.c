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
#define USB_RAW_IOCTL_INIT		_IOW('U', 0, struct usb_raw_init)
#define USB_RAW_IOCTL_RUN		_IO('U', 1)
#define USB_RAW_IOCTL_EVENT_FETCH	_IOR('U', 2, struct usb_raw_event)

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
	arg.speed = USB_SPEED_SUPER; // usb 3.0

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
	fprintf(stdout, "    ----------------------------------------------------------------\n");
	// USB directions (1/3 of bRequestType)
	fprintf(stdout, "    bRequestType:\t0x%x\n\t\t\t[%s]\n",
			event->bRequestType,
			(event->bRequestType & USB_DIR_IN) ? "IN - to host" : "OUT - to device");
	// USB types (2/3 of bRequestType)	
	switch (event->bRequestType & USB_TYPE_MASK) {
		case USB_TYPE_STANDARD:
			fprintf(stdout, "\t\t\tUSB_TYPE_STANDARD:");
			// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L72
			switch (event->bRequest) {
				case USB_REQ_GET_STATUS:
					fprintf(stdout, "\tUSB_REQ_GET_STATUS\n");
					break;
				case USB_REQ_CLEAR_FEATURE:
					fprintf(stdout, "\tUSB_REQ_CLEAR_FEATURE\n");
					break;
				case USB_REQ_SET_FEATURE:
					fprintf(stdout, "\tUSB_REQ_SET_FEATURE\n");
					break;
				case USB_REQ_SET_ADDRESS:
					fprintf(stdout, "\tUSB_REQ_SET_ADDRESS\n");
					break;
				case USB_REQ_GET_DESCRIPTOR:
					// Standard descriptors:
					// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L221
					fprintf(stdout, "\tUSB_REQ_GET_DESCRIPTOR");
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
					fprintf(stdout, "\tUSB_REQ_SET_DESCRIPTOR\n");
					break;
				case USB_REQ_GET_CONFIGURATION:
					fprintf(stdout, "\tUSB_REQ_GET_CONFIGURATION\n");
					break;
				case USB_REQ_SET_CONFIGURATION:
					fprintf(stdout, "\tUSB_REQ_SET_CONFIGURATION\n");
					break;
				case USB_REQ_GET_INTERFACE:
					fprintf(stdout, "\tUSB_REQ_GET_INTERFACE\n");
					break;
				case USB_REQ_SET_INTERFACE:
					fprintf(stdout, "\tUSB_REQ_SET_INTERFACE\n");
					break;
				case USB_REQ_SYNCH_FRAME:
					fprintf(stdout, "\tUSB_REQ_SYNCH_FRAME\n");
					break;
				case USB_REQ_SET_SEL:
					fprintf(stdout, "\tUSB_REQ_SET_SEL\n");
					break;
				case USB_REQ_SET_ISOCH_DELAY:
					fprintf(stdout, "\tUSB_REQ_SET_ISOCH_DELAY\n");
					break;
				default:
					fprintf(stdout, "\tUNKNOWN USB REQ = %d\n", (int)event->bRequest);
					break;
			}
			break;
		case USB_TYPE_CLASS:
			fprintf(stdout, "\t\t\tUSB_TYPE_CLASS\n");
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
	fprintf(stdout, "    ----------------------------------------------------------------\n");
	fprintf(stdout, "\t bRequest:\t0x%x\n", event->bRequest);
	fprintf(stdout, "\t wValue:\t0x%x\n", event->wValue);
	fprintf(stdout, "\t wIndex:\t0x%x\n", event->wIndex);
	fprintf(stdout, "\t wLength:\t%d\n", event->wLength);
}

void event_log(struct usb_raw_event *event) {
	fprintf(stdout, "[i] EVENT TYPE: %d, LENGTH: %u\n", event->type, event->length);
	if (event->type == USB_RAW_EVENT_CONTROL) 
		ctrl_log((struct usb_ctrlrequest *)event->data);
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
			fprintf(stdout, "[i] USB_RAW_EVENT_CONNECT received\n");
		}
		if (event.inner_event.type == USB_RAW_EVENT_CONTROL) {
			fprintf(stdout, "[i] USB_RAW_EVENT_CONTROL received\n");
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
