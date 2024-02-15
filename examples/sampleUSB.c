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

// struct usb_ctrlrequest is at
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L210
// - contains SETUP data for a USB device control request
// - matches fields of the USB 2.0 Spec
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

void usb_loop(int fd) {
	// endless loop
	int n = 0;
	while (n < 10) {
		// distinguishing control event
		struct usb_control_event event;
		event.inner_event.type = 0;
		event.inner_event.length = sizeof(struct usb_control_event);
		// fetch event
		usb_fetch(fd, (struct usb_raw_event *)&event);
		if (event.inner_event.type == USB_RAW_EVENT_CONNECT) {
			fprintf(stdout, "[i] USB_RAW_EVENT_CONNECT received\n");
			n++;
		}
		if (event.inner_event.type == USB_RAW_EVENT_CONTROL)
			fprintf(stdout, "[i] USB_RAW_EVENT_CONTROL received\n");
	}
}

int main()
{
	int fd;
	// 1. create a Raw Gadget instance by opening /dev/raw-gadget
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
