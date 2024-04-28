# USB fuzzing extension to kAFL
Currently, the [kAFL](https://github.com/IntelLabs/kAFL) kernel fuzzing framework cannot fuzz the kernel ***externally*** (i.e., fuzzing the `network` or `usb` kernel stacks from the external side). The primary objective of this diploma project is to extend the existing kAFL framework to support the fuzzing of the USB host kernel stack. Leveraging the syzkaller's custom-implemented [raw gadget](https://github.com/google/syzkaller/blob/master/docs/linux/external_fuzzing_usb.md) interface, we have developed a **prototype of an emulated USB fuzzing device**. Unlike other existing USB fuzzing solutions, this device fuzzes the USB subsystem in the ***vertical*** direction by focusing on the specific USB protocol (such as HID) rather than
fuzzing the descriptor data during the enumeration phase. 

### implementation details
`fuzzer.c` - emulates a fuzzing USB device, utilizing the raw gadget kernel interface coupled with the kAFL *harness*

`fuzzer.h.` - contains the definitions of the raw gadget `ioctl`-based interface, USB structures, and enumerations

`constants.h` - contains the definitions of the USB constants integral to the operations of both `fuzzer.c` and `fuzzer.h`

`usbmon.c` - a program that collects the USB traffic through the `usbmon` kernel module and saves it as binary data (more in [README.md](src/usbmon/README.md))

`ip_range.py` - a program that identifies the IP range of the kernel functions within the compiled Linux kernel binary. it is used to identify of the USB and HID code IP ranges within the kernel (more in [README.md](src/ip_range/README.md)).
