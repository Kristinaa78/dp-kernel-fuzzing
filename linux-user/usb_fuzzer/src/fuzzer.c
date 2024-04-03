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

#include <asm/nyx_api.h>

#include "constants.h"
#include "fuzzer.h"

int setup_usb_device(int fd);
int send_usb_request(int fd);

// [https://intellabs.github.io/kAFL/tutorials/linux/dvkm/agent.html#initialization]
kAFL_payload* kafl_init()
{
    host_config_t host_config = {0};
    agent_config_t agent_config = {0};
    kAFL_payload* buffer = NULL;
    uint64_t ip_range[3] = {0};
    uint64_t ip_range2[3] = {0};
    unsigned long start2 = 0xffffffff82cb83a0;      // address of usb_ep_type_string (via nm)
    unsigned long start = 0xffffffffc0000000;
    unsigned long end2   = 0xffffffff82d1b710;      // address of usb_ep_type_string (via nm)
    unsigned long end   = 0xffffffffc0005000;      // address of usb_ep_type_string (via nm)
    
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
    ip_range2[0] = start2;
    ip_range2[1] = end2;
    ip_range2[2] = 1;
    hprintf("[i] ATTEMPT TO SUBMIT IP RANGE: %lx - %lx [%d]\n", start2, end2, ip_range2[2]); 
    kAFL_hypercall(HYPERCALL_KAFL_RANGE_SUBMIT, (uint64_t)ip_range2);

    // 8. submit CR3
    kAFL_hypercall(HYPERCALL_KAFL_SUBMIT_CR3, 0);
    // [32B vs 64B] - this influences QEMU and libxdc decoder
    kAFL_hypercall(HYPERCALL_KAFL_USER_SUBMIT_MODE, KAFL_MODE_64);

    return buffer;
}

int main() {
    int fd;
    unsigned long ioctl_code, ioctl_num;

    // USB device initialization ----------------------------------
    fd = usb_open();
    hprintf("[i] USB fuzzer started\n");
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
    hprintf("[i] -----------------------------\n");
    // enumeration ------------------------------------------------
    hprintf("[i] start of device configuration:\n");

    // [WIP]
    return -1;

    // kAFL handshake
    kAFL_payload *payload = kafl_init(); 
    // kAFL harness
    /*
    kAFL_hypercall(HYPERCALL_KAFL_NEXT_PAYLOAD, 0); // - takes snapshot on the 1st call
    kAFL_hypercall(HYPERCALL_KAFL_ACQUIRE, 0);
    // ioctl(fd, ioctl_num, &io_buffer);
    kAFL_hypercall(HYPERCALL_KAFL_RELEASE, 0);
    return 0; */
}

int setup_usb_device(int fd) {
    // TO-DO
    // configure USB device/interface/configuration descriptors with IOCTLs
    fprintf(stdout, "[i] fd=%d\n", fd);
    return 0;
}

int send_usb_request(int fd) {
    // TO-DO
    // sending USB requests/data to the host
    fprintf(stdout, "[i] fd=%d\n", fd);
    return 0;
}
