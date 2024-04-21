// raw gadget constants ------------------------------------------------------------
#define DEV_RAW_GADGET 		    "/dev/raw-gadget"
// UDC device name [retrieved by 'ls /sys/class/udc/']
#define DEVICE_NAME  		    "dummy_udc.0"
// UDC driver name [retrieved by 'cat /sys/class/udc/dummy_udc.0/uevent']
#define DRIVER_NAME  		    "dummy_udc"
#define UDC_NAME_LENGTH_MAX     128
// EP-related constants
#define USB_RAW_EP_ADDR_ANY     0xff
#define USB_RAW_EPS_NUM_MAX     30
#define USB_RAW_EP_NAME_MAX     16
#define EP_NUM_INT_IN           0x0
#define EP_MAX_PACKET_INT       8
#define EP0_MAX_DATA            256
#define EP_MAX_PACKET_CONTROL   64
#define EP_MAX_PACKET_INT       8
#define UDC_NAME_LENGTH_MAX     128
#define USB_MAX_PACKET_SIZE     128
// generic constants
#define MAX_INTERFACE_NUMBER	1
#define MAX_EPS_NUMBER		32
// USB device descriptor-related constants  [https://www.beyondlogic.org/usbnutshell/usb5.shtml]
// - highest version of the USB specification the device supports
#define BCD_USB		0x0200 // USB 2.0
// - vendor and product ID are used to find a driver
// - vendor ID assigned by the USB-IF
// [https://the-sz.com/products/usbid/index.php]
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
#define USB_HID_HID		            0x21
#define USB_HID_REPORT	            0x22
#define USB_HID_PHYS	            0x23
#define USB_HID_DESCRIPTOR_SIZE     0x09
