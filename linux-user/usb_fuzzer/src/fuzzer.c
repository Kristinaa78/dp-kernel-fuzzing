#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>

#include "fuzzer.h"
#include <asm/nyx_api.h>

// USB device descriptor -------------------------------------
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L286
// - describes general information about a USB device
// - high-speed device also needs device_qualifier descriptor
struct usb_device_descriptor device = {
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

// USB configuration descriptor-------------------------------
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L346
// - describes information about a specific device configuration
// - device has one or more such descriptors
// - each configuration has one or more interfaces
// - each interface has zero or more endpoints
struct usb_config_descriptor config = {
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

// USB interface descriptor ----------------------------------
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L389
// - describes a specific interface within a configuration
// - has zero or more endpoint descriptors (without EP0)
// - always returned as part of the configuration descriptor (interface desc. cannot be
// directly accessed)
// - SetInterface()/GetInterface() work only with alternative interfaces
struct usb_interface_descriptor interface = {
	.bLength =				USB_DT_INTERFACE_SIZE,	// descriptor's size
	.bDescriptorType =		USB_DT_INTERFACE,
	.bInterfaceNumber =		0, // interface's number: index in the array of supported interfaces
	.bAlternateSetting = 	0, // value specifies alternate setting for the interface
	.bNumEndpoints =		1, // number of endpoints (if 0, interface only uses Default Control Pipe)
	.bInterfaceClass =		0, // USB_CLASS_HID, // class code
	.bInterfaceSubClass =	1, // subclass code
	.bInterfaceProtocol =	1, // subclass/class-specific protocol code
	.iInterface =			STRING_ID_INTERFACE,
};

// USB endpoint descriptor -----------------------------------
// https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/usb/ch9.h#L407
// - describes information about the bandwidth requirements of the endpoint
// - always returned as part of the configuration information
// - cannot be directly accessed
// - EP0 has no descriptor
// - misses 2 fields (bRefresh and bSynchAddress used in audio)
struct usb_endpoint_descriptor endpoint = {
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

// USB HID specifics ------------------------------------------
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

struct custom_descriptors descriptors = {
	.device 	= &device,
	.config 	= &config,
	.interface 	= &interface,
	.endpoint 	= &endpoint,
	.hid 		= NULL
};

int ep_int_in = -1;
pthread_t ep_int_in_thread;
int ep_int_in_thread_spawned = 0;

// kAFL handshake --------------------------------------------
// [https://intellabs.github.io/kAFL/tutorials/linux/dvkm/agent.html#initialization]
kAFL_payload* kafl_init()
{
    host_config_t host_config = {0};
    agent_config_t agent_config = {0};
    kAFL_payload* buffer = NULL;
    uint64_t ip_range[3] = {0};
    //unsigned long start  = 0xffffffff82cb83a0;      // address of usb_ep_type_string (via nm)
    //unsigned long start  = 0xffffffffc0201000;      // address of usb_ep_type_string (via nm)
	//unsigned long end    = 0xffffffffc0208000;      // address of usb_ep_type_string (via nm)
	unsigned long start = 0xffffffff8117daa0;
	unsigned long end = 0xffffffff860c92e0;
    hprintf("[i] kAFL agent initialization\n");
    
    // 1. kAFL handshake
    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);
    
    // 2. query host config
    kAFL_hypercall(HYPERCALL_KAFL_GET_HOST_CONFIG, (uintptr_t)&host_config);
    hprintf("[i] HOST CONFIG:\n");
    hprintf("\thost_magic:            %d [0x%x]\n", host_config.host_magic, host_config.host_magic);
    hprintf("\thost_version:          %d [0x%x]\n", host_config.host_version, host_config.host_version);
    hprintf("\tbitmap_size:           %d [%dKB]\n", host_config.bitmap_size, host_config.bitmap_size / 1024);
    hprintf("\tijon_bitmap_size:      %dB\n", host_config.ijon_bitmap_size);
    hprintf("\tpayload_buffer_size:   %d [%dKB]\n",
            host_config.payload_buffer_size,
            host_config.payload_buffer_size / 1024);
    hprintf("\tworker_id:             %d\n", host_config.worker_id);
   
    // sanity checks - constants defined at:
    // [https://github.com/IntelLabs/kafl.targets/blob/master/nyx_api.h#L146]
    if (host_config.host_magic != NYX_HOST_MAGIC) {
        hprintf("[-] INCOMPATIBLE MAGIC NUMBERS: 0x%x != 0x%x\n", host_config.host_magic, NYX_HOST_MAGIC);
        habort("INCOMPATIBLE MAGIC NUMBERS\n");
    }
    // [https://github.com/IntelLabs/kafl.targets/blob/master/nyx_api.h#L149]
    if (host_config.host_version != NYX_HOST_VERSION) {
        hprintf("[-] INCOMPATIBLE VERSIONS: 0x%x != 0x%x\n", host_config.host_version, NYX_HOST_VERSION);
        habort("INCOMPATIBLE VERSIONS\n");
    }

    // 3. set guest agent config
    // [https://github.com/IntelLabs/kafl.targets/blob/master/nyx_api.h#L147]
    agent_config.agent_magic = NYX_AGENT_MAGIC;
    // [https://github.com/IntelLabs/kafl.targets/blob/master/nyx_api.h#L150]
    agent_config.agent_version = NYX_AGENT_VERSION;
    // if set, disables VM snapshotting
    agent_config.agent_non_reload_mode = 0;
    agent_config.coverage_bitmap_size = host_config.bitmap_size; 
    hprintf("[i] AGENT CONFIG:\n");
    hprintf("\tagent_magic:           %d [0x%x]\n", agent_config.agent_magic, agent_config.agent_magic);
    hprintf("\tagent_version:         %d [0x%x]\n", agent_config.agent_version, agent_config.agent_version);
    hprintf("\tagent_non_reload_mode: %d\n", agent_config.agent_non_reload_mode);
    kAFL_hypercall(HYPERCALL_KAFL_SET_AGENT_CONFIG, (uintptr_t)&agent_config);
    
    // 4. allocate page-aligned payload buffer
    buffer = aligned_alloc((size_t)sysconf(_SC_PAGESIZE), host_config.payload_buffer_size);
    if (buffer == NULL) habort("PAYLOAD BUFFER ALLOCATION FAILED\n"); 
    mlock(buffer, host_config.payload_buffer_size); // prevents memory from being paged to the SWAP area
    
    // 5. map payload buffer
    kAFL_hypercall(HYPERCALL_KAFL_GET_PAYLOAD, (uintptr_t)buffer); 

    // 6. submit crash handlers [SKIPPED]
    // 7. submit Intel PT ranges
    // [https://intellabs.github.io/kAFL/reference/hypercall_api.html#range-submit]
    // [https://github.com/nyx-fuzz/libxdc?tab=readme-ov-file#warnings] - at least 1 filter range needs
    // to be set (without it, QEMU-NYX fails with "[QEMU-NYX] Error: libxdc_init() has failed")
    // #learnedthatthehardway
    ip_range[0] = start;
    ip_range[1] = end;
    ip_range[2] = 0;
    hprintf("[i] ATTEMPT TO SUBMIT IP RANGE: %lx - %lx [%d]\n", start, end, ip_range[2]); 
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint64_t)ip_range);

    // 8. submit CR3
    kAFL_hypercall(HYPERCALL_KAFL_SUBMIT_CR3, 0);
    // [32B vs 64B] - this influences QEMU and libxdc decoder
    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_64);

    return buffer;
}

// USB fuzzing loop -------------------------------------------
void *fuzzing_loop(void *arg) {
	int fd = (int)(long)arg;
	struct usb_int_io io;
	io.inner_io.ep = ep_int_in;
	io.inner_io.flags = 0;
	io.inner_io.length = EP_MAX_PACKET_INT;

	kAFL_payload *payload = kafl_init();
	while (1) {
		kAFL_hypercall(HYPERCALL_KAFL_NEXT_PAYLOAD, 0);
		kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
		int result = usb_ep_write_may_fail(fd, (struct usb_ep_io *)&io);

		if (result < 0 && errno == ESHUTDOWN) {
			hprintf("[i] ep_int_in: device was likely reset, exiting\n");
			break;
		} else if (result < 0) {
			perror("usb_ep_write_may_fail()");
			return NULL;
		}
		
    	kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);
		sleep(1);
	}

	return NULL;
}


// USB device initialization ----------------------------------
int setup_usb_device() {
    int fd = usb_open();
    if(fd < 0) habort("[-] unable to open /dev/raw-gadget");
    if(usb_init(fd, USB_SPEED_HIGH, DRIVER_NAME, DEVICE_NAME) < 0) {
        close(fd);
        habort("[-] unable to initialize the device");
    }
    hprintf("[i] USB device initialized\n");
    if(usb_run(fd) < 0) {
        close(fd);
        habort("[-] unable to run the device");
    }
    hprintf("[i] USB device is running\n");
    return fd;
}

// USB descriptors setup --------------------------------------
int setup_request(int fd, struct usb_control_event *event, struct usb_control_io *io) {
	// determine request type
	switch (event->ctrl.bRequestType & USB_TYPE_MASK) {
		case USB_TYPE_STANDARD:
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
							hprintf("USB_TYPE_STANDARD --> USB_REQ_GET_DESCRIPTOR --> USB_DT_DEVICE\n");
							memcpy(&io->data[0], descriptors.device, sizeof(*descriptors.device));
							io->inner_io.length = sizeof(*descriptors.device);
							return 1;
						// configuration descriptor
						case USB_DT_CONFIG:
							hprintf("USB_TYPE_STANDARD --> USB_REQ_GET_DESCRIPTOR --> USB_DT_CONFIG\n");
							io->inner_io.length = build_config(&io->data[0], sizeof(io->data), &descriptors);
							return 1;
						// string descriptor
						case USB_DT_STRING:
							hprintf("USB_TYPE_STANDARD --> USB_REQ_GET_DESCRIPTOR --> USB_DT_STRING\n");
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
							hprintf("USB_TYPE_STANDARD --> USB_REQ_GET_DESCRIPTOR --> HID_DT_REPORT\n");
							memcpy(&io->data[0], &usb_hid_report[0], sizeof(usb_hid_report));
							io->inner_io.length = sizeof(usb_hid_report);
							return 1;
						default:
							hprintf("[-] FAILURE: no response\n");
							return 0;
					}
					break;
				// when host queries previously set configuration
				case USB_REQ_GET_CONFIGURATION:
					hprintf("USB_TYPE_STANDARD --> USB_REQ_GET_CONFIGURATION\n");
					io->inner_io.length = build_config(&io->data[0], sizeof(io->data), &descriptors);
					return 1;
				// when driver tries to configure the device
				// usually wValue == 1 (selects 1st config)
				// if wValue == 0, device should be deconfigured
				case USB_REQ_SET_CONFIGURATION:
 					hprintf("USB_TYPE_STANDARD --> USB_REQ_SET_CONFIGURATION\n");
					ep_int_in = usb_ep_enable(fd, descriptors.endpoint);
					hprintf("\t |-- EP0: ep_int_in enabled: %d\n", ep_int_in);
					// spawns new thread
					// - invokes ep_int_in_loop as its start_routine() with fd as arg
					// - thread terminates if it calls pthread_exit(),  returns from
					// start_routine(), main thread exits(), or gets canceled
					int result = pthread_create(&ep_int_in_thread, 0, fuzzing_loop,  (void*)(long)fd);
					if (result != 0) {
						habort("[-] UNABLE TO CRETE THREAD (ep_int_in)");	
						return 0;
					}
					ep_int_in_thread_spawned =  1;
					hprintf("[+] EP0: SPAWNED THREAD (ep_int_in)\n");
					configure_device(fd, descriptors.config);
					io->inner_io.length = 0;
					// when new HID device (e.g., keyboard) is added, it is firstly managed by 
					// /drivers/hid/hid-core.c --> hid_add_device()
					return 1;
				case USB_REQ_SET_INTERFACE:
					io->inner_io.length = 0;
					return 1;
				case USB_REQ_GET_INTERFACE:
					io->data[0] = descriptors.interface->bAlternateSetting;
					io->inner_io.length = 1;
					return 1;
				default:
					hprintf("[-] FAILURE: no response\n");
					return 0;
			}
			break;
		case USB_TYPE_CLASS:
			switch (event->ctrl.bRequest) {
				case HID_REQ_SET_REPORT:
					hprintf("USB_TYPE_CLASS --> HID_REQ_SET_REPORT\n");
					io->inner_io.length = 1;
					return 1;
				case HID_REQ_SET_IDLE:
					hprintf("USB_TYPE_CLASS --> HID_REQ_SET_IDLE\n");
					io->inner_io.length = 0;
					return 1;
				case HID_REQ_SET_PROTOCOL:
					hprintf("USB_TYPE_CLASS --> HID_REQ_SET_PROTOCOL\n");
					io->inner_io.length = 0;
					return 1;
				default:
					hprintf("[-] FAILURE: no response\n");
					return 0;
			}
			break;
		default:
			hprintf("[-] FAILURE: no response\n");
			return 0;
	}
	return 0;
}

// USB device enumeration -------------------------------------
void usb_loop(int fd) {
    hprintf("[i] start of the USB device configuration:\n");
	// endless loop
	while (1) {
		// distinguish the CONTROL event
		struct usb_control_event event;
		event.inner_event.type = 0;
		event.inner_event.length = sizeof(struct usb_control_event);
		// fetch USB event
		usb_fetch(fd, (struct usb_event *)&event);
		if (event.inner_event.type == USB_RAW_EVENT_CONNECT) {
			// assign address to the device's endpoint
			usb_endpoints_info(fd, descriptors.endpoint);
			continue;
		}
		// ignore all other events
		if (event.inner_event.type != USB_RAW_EVENT_CONTROL) continue;
		// process CONTROL event
		struct usb_control_io io;
		io.inner_io.ep 		= 0;
		io.inner_io.flags 	= 0;
		io.inner_io.length 	= 0;
		// 
		int reply = setup_request(fd, &event, &io);
		if (!reply) {
			hprintf("[-] EP0 HALTED\n");
			usb_endpoint_stall(fd);
			continue;
		}
		if (event.ctrl.wLength < io.inner_io.length)
			io.inner_io.length = event.ctrl.wLength;
		if (event.ctrl.bRequestType & USB_DIR_IN) {
			int result = usb_ep0_write(fd, (struct usb_ep_io *)&io);
			hprintf("[i] EP0: transferred %d bytes [IN]\n", result);
		} else {
			int result = usb_ep0_read(fd, (struct usb_ep_io *)&io);
			hprintf("[i] EP0: transferred %d bytes [OUT]\n", result);
		}
	}
}

int main() {
    int fd;
    hprintf("[i] USB fuzzer started\n");
	// connect the USB device
	fd = setup_usb_device();
	if (fd < 0) habort("USB device set up FAILED\n");
	// set up and run the USB device
	usb_loop(fd);
    return -1;
}

