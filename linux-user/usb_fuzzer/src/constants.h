#define DEV_RAW_GADGET 		    "/dev/raw-gadget"
// UDC device name [retrieved by 'ls /sys/class/udc/']
#define DEVICE_NAME  		    "dummy_udc.0"
// UDC driver name [retrieved by 'cat /sys/class/udc/dummy_udc.0/uevent']
#define DRIVER_NAME  		    "dummy_udc"
// RAW GADGET CONSTANTS ------------------------------------------------
#define USB_RAW_EP_ADDR_ANY     0xff
#define USB_RAW_EPS_NUM_MAX     30
#define USB_RAW_EP_NAME_MAX     16
// this will be assigned dynamically
#define EP_NUM_INT_IN           0x0
// maximum data payload size on the interrupt endpoint in the default
// configuration is only 64 (maximum is 1024 but in alternative interfaces)
#define EP_MAX_PACKET_INT       8
#define EP0_MAX_DATA            256
#define EP_MAX_PACKET_CONTROL   64
#define UDC_NAME_LENGTH_MAX     128
#define USB_MAX_PACKET_SIZE     128
// USB device descriptor-related constants -----------------------------
// more at [https://www.beyondlogic.org/usbnutshell/usb5.shtml]
// - highest version of the USB specification the device supports
#define BCD_USB		0x0200
// - vendor ID assigned by the USB-IF
// more at [https://the-sz.com/products/usbid/index.php]
#define USB_VENDOR	0x0483 // STMicroelectronics 
#define USB_PRODUCT	0x5232
// (optional) indeces of string descriptors
// - provide details about manufacturer, product, a serial number
#define STRING_ID_MANUFACTURER	0
#define STRING_ID_PRODUCT	1
#define STRING_ID_SERIAL	23
#define STRING_ID_CONFIG	3
#define STRING_ID_INTERFACE	4
// HID-related constants
#define USB_HID_HID 		0x21
#define USB_HID_REPORT	        0x22
#define USB_HID_PHYS	        0x23
#define USB_HID_DESCRIPTOR_SIZE 0x09
