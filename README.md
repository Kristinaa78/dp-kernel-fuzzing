
## usbmon - tracing USB data
Program `usbmon.c` collects and saves USB traffic in binary format.
It interacts with the `/dev/usbmonX` character device provided by the `usbmon` Linux kernel module. 

`usbmon` kernel module must be included in the kernel config (`CONFIG_USB_MON=y`) and loaded
into the system (with `sudo modprobe usbmon`). As its
[documentation](https://docs.kernel.org/usb/usbmon.html) states, the USB traffic can be collected
either in human-readable text format via `debugfs` or as raw binary data through `usbmon`'s IOCTL
kernel API. While the first approach is limited regarding the output's size, IOCTLs give us access to all data.


## usage
Load `usbmon` kernel module with:
```
# modprobe usbmon
```
Check the output of `lsusb -t` and select the bus you want to trace.
Check if corresponding `/dev/usbmon{bus number}` exists.

Compile `usbmon.c` with:
```
gcc -o usbmon usbmon.c -O2 -Wall
```

Run the program as a superuser:
```
# ./usbmon {/dev/usbmonX} {output_file_path}
```
If no CLI argument is provided, the program opens `/dev/usbmon0` (which monitors traffic on all buses)
and `usbmon_data.txt` as default. If one wants
to redirect output to another file, both arguments must be provided.

Once you start the program, perform desired operations that will create USB traffic (e.g., copy large 
files onto a USB drive, start a webcam, plug in a keyboard, or make movements with a mouse). All traffic
is also printed to `stdout`.

## output
Binary data is captured and stored in binary files. The final output includes only the fields relevant
to fuzzing from all the `usbmon_packet`'s fields. In the output file, `struct custom_usb` is stored, and
followed by the payload of `len_cap` size.

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
```
