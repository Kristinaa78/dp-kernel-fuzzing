# USB Kernel Fuzzing with kAFL

This repository accompanies a master's thesis on **experimental USB fuzzing of the Linux kernel** using
the **[kAFL](https://github.com/IntelLabs/kAFL) fuzzer**. kAFL is a coverage-guided, hardware-assisted
kernel fuzzer that leverages **Intel Processor Trace (Intel PT)** to collect feedback while
fuzzing kernel code inside a virtualized target.

The goal of the work is to fuzz the Linux kernel's **USB stack** by feeding
the kernel generated USB traffic and using Intel PT feedback to explore the USB subsystem's code paths in
search of bugs.

The presented solution is an **enhancement to the kAFL fuzzing framework**.

## My contributions

The kAFL fuzzer itself was **not** modified. It is included only as cloned code so that the fuzzing
environment runs (see [Repository structure](#repository-structure) below). The original work of this
thesis lives in the following places:

- **USB stack fuzzing target** —
  [`linux-user/usb_fuzzer`](https://github.com/Kristinaa78/dp-kernel-fuzzing/tree/kAFL-targets/linux-user/usb_fuzzer)
  (branch `kAFL-targets`)
  is the code that fuzzes the Linux kernel USB stack: the harness/target that delivers fuzzed USB input to the
  kernel under kAFL.
- **USB activity logging utilities** —
  [`usbmon` branch](https://github.com/Kristinaa78/dp-kernel-fuzzing/tree/usbmon) contains utilities for logging real USB communication activity, used to generate seed/input data for fuzzing.
- **Custom synthetic USB device** —
  [`examples/sampleUSB.c`](https://github.com/Kristinaa78/dp-kernel-fuzzing/blob/raw-gadget/examples/sampleUSB.c)
  (branch `raw-gadget`)
 is the code for generating a custom, artificial USB device used in the experiments.

## Repository structure

The repository is a copy of several kAFL GitHub repositories spread across branches. The upstream copies
are unmodified; the thesis extension lives in the `kAFL-targets` branch (plus the supporting `usbmon` and
`raw-gadget` branches).

| Branch | Contents | Origin |
|--------|----------|--------|
| `main` | Copy of [IntelLabs/kAFL](https://github.com/IntelLabs/kAFL). | upstream |
| `kAFL-fuzzer` | Copy of [IntelLabs/kafl.fuzzer](https://github.com/IntelLabs/kafl.fuzzer). | upstream |
| `kAFL-targets` | Copy of [IntelLabs/kafl.targets](https://github.com/IntelLabs/kafl.targets), containing the thesis **extension** (the USB fuzzing target). | upstream + thesis |
| `usbmon` | Utilities for collecting real USB data (the fuzzing corpus). | thesis |
| `raw-gadget` | Code for generating a custom, artificial USB device. | thesis |

### The extension (`kAFL-targets` branch)

The core of the implementation resides in `linux-user/usb_fuzzer`:

- **`fuzzer.c`** — the core of the extension: code for the emulated fuzzing USB device. Accompanied by its
  header files **`fuzzer.h`** and **`constants.h`**.
- **`ip_range.py`** — a script that retrieves the specified target IP ranges from the compiled `vmlinux`
  binary.
- **`usb_functions.txt`** and **`hid_functions.txt`** — the lists of functions for the USB host-side API
  and the USB HID core, respectively.

### Supporting utilities

- **`usbmon.c`** (branch `usbmon`) — collects actual USB data (the fuzzing corpus) traced by the `usbmon`
  kernel module.
- **`examples/sampleUSB.c`** (branch `raw-gadget`) — generates a custom, artificial USB device used in the
  experiments.

## Requirements

kAFL relies on hardware and platform support, most notably:

- An Intel CPU with **Intel Processor Trace (Intel PT)** support
- A kAFL-compatible host setup (KVM/Nyx backend) as described in the upstream kAFL documentation

Refer to the upstream [kAFL documentation](https://github.com/IntelLabs/kAFL) for the full host
prerequisites and setup steps.

## Documentation
The complete background on USB fuzzing, the design of the fuzzing target and supporting
utilities, the experimental setup, and the results is available in [`dp.pdf`](dp.pdf). A detailed user
manual is included at the end of the document.

## Disclaimer

This project is academic research carried out as part of a master's thesis. The fuzzing harness, USB device
emulation, and logging utilities are intended for use in an isolated research environment for the purpose of
finding and studying bugs in the Linux kernel USB stack. They are not meant for use against systems you do
not own or are not authorized to test.


***

<h1 align="center">
  <br>kAFL</br>
</h1>

<h3 align="center">
HW-assisted Feedback Fuzzer for x86 VMs
</h3>

<p align="center">
  <a href="https://github.com/IntelLabs/kAFL/actions/workflows/CI.yml">
    <img src="https://github.com/IntelLabs/kAFL/actions/workflows/CI.yml/badge.svg" alt="CI">
  </a>
  <a href="https://github.com/IntelLabs/kAFL/releases">
    <img alt="GitHub release (latest by date)" src="https://img.shields.io/github/v/release/IntelLabs/kAFL">
  </a>
  <a href="https://hub.docker.com/r/intellabs/kafl">
    <img alt="Docker Image Version (latest by date)" src="https://img.shields.io/docker/v/intellabs/kafl?label=Docker%20Image">
  </a>
  <a href="https://hub.docker.com/r/intellabs/kafl">
    <img alt="Docker Pulls" src="https://img.shields.io/docker/pulls/intellabs/kafl">
  </a>
  <a href="https://github.com/IntelLabs/kAFL/blob/master/LICENSE.md">
    <img alt="GitHub" src="https://img.shields.io/github/license/IntelLabs/kafl">
  </a>
</p>
<p align="center">
  <a href="https://IntelLabs.github.io/kAFL/">
    <img src="https://img.shields.io/badge/Online-Documentation-green?style=for-the-badge&logo=gitbook" alt="online_docs"/>
  </a>
</p>

kAFL/[Nyx](https://nyx-fuzz.com) is a fast guided fuzzer for the x86 VM. It is great for anything that
executes as QEMU/KVM guest, in particular x86 firmware, kernels and full-blown
operating systems.

**Note: All components are provided for research and validation purposes only.
Use at your own Risk**

## Targets

kAFL is the main fuzzer driving the [**Linux Security Hardening for Confidential Compute**](https://github.com/intel/ccc-linux-guest-hardening) effort, identifing vulnerabilities in a complex setup and improving the security of the Linux kernel for all CC solutions.

Among other successful targets for kAFL/Nyx :

- [**Intel SGX enclaves**](https://www.usenix.org/conference/usenixsecurity22/presentation/cloosters)
- [**Intel TDX TDVF firmware**](https://github.com/hemx0147/TDVFuzz)
- [**Mozilla Firefox IPCs**](https://dl.acm.org/doi/10.1145/3492321.3519591)
- [**Linux network applications** ](https://dl.acm.org/doi/10.1145/3492321.3519591)
- [**Windows drivers**](https://github.com/IntelLabs/kAFL/issues/53)
- [**Hypervisors**](https://www.usenix.org/conference/usenixsecurity21/presentation/schumilo)
- Play [**Super Mario** at 10-30x speedups](https://dl.acm.org/doi/10.1145/3492321.3519591) !

Additionally, kAFL has been used internally at Intel for x86 firmware and drivers validation as well as SMM handlers fuzzing.

## Features

- kAFL/Nyx uses [_Intel VT_](https://www.intel.com/content/www/us/en/virtualization/virtualization-technology/intel-virtualization-technology.html), [_Intel PML_](https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/page-modification-logging-vmm-white-paper.pdf) and _Intel PT_ to achieve efficient execution, snapshot reset and coverage feedback for greybox or whitebox fuzzing scenarios. It allows to run many x86 FW and OS kernels with any desired toolchain and minimal code 
modifications.

- kAFL uses a custom [kAFL-Fuzzer](https://github.com/IntelLabs/kafl.fuzzer)
  written in Python. The kAFL-Fuzzer follows an AFL-like design and is optimized
  for working with many Qemu instances in parallel, supporting flexible VM
  configuration, logging and debug options.

- kAFL integrates the [_Radamsa_](https://gitlab.com/akihe/radamsa) fuzzer as well as [_Redqueen_](https://github.com/RUB-SysSec/redqueen) and [_Grimoire_](https://github.com/RUB-SysSec/grimoire) extensions. Redqueen uses VM introspection to extract runtime inputs to conditional instructions, overcoming typical magic byte and other input checks. Grimoire attempts to identify keywords and syntax from fuzz inputs in order to generate more clever large-scale mutations.

For details on **Redqueen**, **Grimoire**, [_IJON_](https://github.com/RUB-SysSec/ijon), **Nyx**, please visit [nyx-fuzz.com](https://nyx-fuzz.com).


## Requirements

- **Intel Skylake or later:** The setup requires a Gen-6 or newer Intel CPU (for
  Intel PT) and adequate system memory (~2GB RAM per CPU)

- **Patched Host Kernel:** A modified Linux host kernel will be installed as part
  of the setup. Running kAFL inside a VM may work starting IceLake or later CPU.

- **Recent Debian/Ubuntu:** The installation and tutorials are
  tested for recent Ubuntu LTS (>=20.04) and Debian (>=bullseye).


## Getting Started

Once you have python3-venv and make installed, you can install kAFL using `make deploy`:

```shell
sudo apt install python3-venv make git
git clone https://github.com/IntelLabs/kAFL.git
cd kAFl
make deploy
```

Installation make take some time and require a reboot to update your kernel.

Check the detailed [installation guide](https://intellabs.github.io/kAFL/tutorials/installation.html) in case
of trouble, or the [deployment guide](https://intellabs.github.io/kAFL/reference/deployment.html) for detailed
information and customizing the kAFL setup for your project.

## Fuzzing your first target

As a first fuzzing example, we recommend [Fuzzing the Linux Kernel](https://intellabs.github.io/kAFL/tutorials/fuzzing_linux_kernel.html).

Other targets are available such as:

- [Windows driver/userspace](https://intellabs.github.io/kAFL/tutorials/windows/index.html)
- [Linux userspace](https://github.com/IntelLabs/kafl.targets/tree/master/linux-user)
- [UEFI OVMF](https://github.com/IntelLabs/kafl.targets/tree/master/uefi_ovmf_64)

A improved documentation is under work for these targets.

## Maintainers

- [@Wenzel - Mathieu Tarral](https://github.com/Wenzel) ([Intel](https://github.com/IntelLabs))
- [@il-steffen - Steffen Schulz](https://github.com/il-steffen) ([IntelLabs](https://github.com/IntelLabs))

## License

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
