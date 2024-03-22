
## usbmon - tracing USB data
Program `usbmon.c` collects and saves USB traffic in binary format. It interacts with `/dev/usbmonX` character device provided by `usbmon` Linux kernel module. 

`usbmon` kernel module needs to be included in the kernel config (`CONFIG_USB_MON=y`) and loaded into system (with `sudo modprobe usbmon`). As its [documentation](https://docs.kernel.org/usb/usbmon.html) states, the USB traffic can be collected either in human-readable text format via `debugfs` or as raw binary data through `usbmon`'s IOCTL kernel API. While the first approach is limited in terms of the output's size, IOCTLs give us access to all data.


## usage
Load `usbmon` kernel module with:
```
# modprobe usbmon
```
Check the output of `lsusb -t` and select the bus you want to trace. Check if corresponding `/dev/usbmon{bus number}` exists.

Compile `usbmon.c` with:
```
gcc -o usbmon usbmon.c -O2 -Wall
```

Run the program as a superuser:
```
# ./usbmon {/dev/usbmonX}
```
If no CLI argument is provided, program opens `/dev/usbmon0` as default.
## output
Binary data is captured and stored in binary files. From all the `usbmon_packet`'s fields only the ones relevant for fuzzing are included in the final output. In the output file, `struct custom_usb` is stored and followed by the payload of `len_cap` size.

```
struct custom_usb {
    uint8_t  type;
    uint8_t  xfer_type;
    uint8_t  epnum;
    uint8_t  devnum;
    uint16_t busnum;
    char     flag_data;
    int32_t  status;
    uint32_t length;
    uint32_t len_cap;
    uint32_t xfer_flags;
};
```