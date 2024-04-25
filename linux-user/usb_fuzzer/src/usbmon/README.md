## usbmon - tracing USB data
Program `usbmon.c` collects and saves USB traffic in binary format. It interacts with `/dev/usbmonX` character device provided by `usbmon` Linux kernel module. 

`usbmon` kernel module needs to be included in the kernel config (`CONFIG_USB_MON=y`) and loaded into system (with `sudo modprobe usbmon`). As its [documentation](https://docs.kernel.org/usb/usbmon.html) states, the USB traffic can be collected either in human-readable text format via `debugfs` or as raw binary data through `usbmon`'s IOCTL kernel API. While the first approach is limited in terms of the output's size, IOCTLs give us access to all data.


## usage
Load `usbmon` kernel module with:
```
# modprobe usbmon
```

Compile `usbmon.c` with:
```
gcc -o usbmon usbmon.c -O2 -Wall
```

Check the output of `lsusb` and select the bus you want to trace. Check if corresponding `/dev/usbmon{bus number}` exists. With `devnum`, one can also filter between devices on the bus. For instance:
```
11:30  @ usbmon: lsusb
Bus 003 Device 013: ID 0483:5232 STMicroelectronics 68EC-S
```

Run the program as a superuser:
```
# ./usbmon --bus /dev/usbmon3 --devnum 13
```
If no CLI `--bus` argument is provided, program opens `/dev/usbmon0` as default and filters no devices.
## output
Binary data is captured, printed to `stdout`, and data payloads are stored as binary files under
`data/` directory. However, the source code can be easily modified to include any of the
packet's fields:
```
struct custom_usb {
    uint8_t     type;           // event type ([S]ubmission/[C]allback/[E]rror)
    uint8_t     xfer_type;      // transfer type
    uint8_t     epnum;          // endpoint number/direction
    uint8_t     devnum;         // device address
    uint16_t    busnum;         // bus number
    char        flag_data;      // if data is present
    int32_t     status;         // URB status
    uint32_t    length;         // length of data
    uint32_t    len_cap;        // delivered length
    uint32_t    xfer_flags;     // copy of URB's transfer flags
};
