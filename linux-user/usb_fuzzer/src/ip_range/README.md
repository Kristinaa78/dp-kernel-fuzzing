
# Identification of static kernel IP range for USB fuzzing
[kAFL](https://github.com/IntelLabs/kAFL) uses [Intel PT](https://www.intel.com/content/www/us/en/support/articles/000056730/processors.html) tracing to collect  code coverage information. As the collected information is highly compressed, kAFL uses the [libxdc](https://github.com/nyx-fuzz/libxdc) decoding library internally. When fuzzing the Linux kernel, an IP (instruction pointer) range filter [must be set](https://github.com/nyx-fuzz/libxdc?tab=readme-ov-file#warnings) during the kAFL initialization so that only relevant execution paths are captured.

### setup
To determine the IP range for USB host stack fuzzing, ensure the Linux kernel is compiled with the debugging symbols by enabling `CONFIG_DEBUG_INFO` in the kernegg. kAFL's [target Linux](https://github.com/IntelLabs/kafl.linux/tree/6521682f674d0c720936a2c22b3967667ad6c70b) has it enabled by default.

Additionally, have the following utilities/programs installed:
- `gdb` 
- `nm`
- `python` (v3.x) 

### USB host stack API
From the [official documentation](https://www.kernel.org/doc/html/latest/driver-api/usb/usb.html), a list of USB host-side API kernel functions has been obtained (see `usb_functions.txt`). The majority of these are implemented in `/drivers/usb/core`. The addresses can be easily retrieved by using the `nm` utility on a statically linked Linux kernel executable (`vmlinux`):
```
05:13  @ linux_kafl_agent: nm vmlinux | grep 'usb_submit_urb'
ffffffff849bd1ca r __kstrtabns_usb_submit_urb
ffffffff849da7e1 r __kstrtab_usb_submit_urb
ffffffff849b2694 r __ksymtab_usb_submit_urb
ffffffff82ce4a30 T __pfx_usb_submit_urb
ffffffff82ce4a40 T usb_submit_urb
```
The provided script (`usb_functions.py`) reads all symbolic information within `vmlinux` with `nm` utility, searches for the kernel functions specified in `usb_functions.txt`, and produces a well-formatted output file called `functions_addresses.txt`. Retrieved addresses are sorted (from lowest to highest), and each line contains the address and function name:
```
0xffffffff82cb83a0: usb_ep_type_string
0xffffffff82cb8460: usb_speed_string
0xffffffff82cb84c0: usb_state_string
0xffffffff82cb8520: usb_decode_interval
```
Size of the last function needs to be added to the highest address. Alternatively, this new highest value can be obtained with `gdb`.
With such information, we can quickly determine the IP range \[lowest-highest address\] and implement it in the fuzzer (with a dedicated hypercall `RANGE_SUBMIT`). 

The code of these kernel functions stretches across a continual range of memory addresses. This can be easily checked with `gdb`:

```
05:26  @ linux_kafl_agent: gdb vmlinux 
GNU gdb (Ubuntu 12.1-0ubuntu1~22.04) 12.1
.....
(gdb) x/20i usb_ep_type_string
   0xffffffff82cb83a0 <usb_ep_type_string>:	endbr64 
   0xffffffff82cb83a4 <usb_ep_type_string+4>:	cmp    $0x3,%edi
   0xffffffff82cb83a7 <usb_ep_type_string+7>:	ja     0xffffffff82cb83da <usb_ep_type_string+58>
   ....
   // repeat the command with a blank line until the end of the IP range
```
For obvious reasons, the kernel needs to be run with disabled KASLR. This can be done by specifying the `nokaslr` kernel boot parameter in QEMU (in `-append` option). `nokaslr` parameter disables the kernel randomization which can be verified by either debugging the running system with `gdb` or by using `grep` against `/proc/kallsyms` in VM.





