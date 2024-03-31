#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>

#include <asm/nyx_api.h>

#include "constants.h"
#include "fuzzer.h"

int setup_usb_device(int fd);
int send_usb_request(int fd);

// kAFL initialization function - follows the kAFL initialization protocol
// [https://intellabs.github.io/kAFL/tutorials/linux/dvkm/agent.html#initialization]
kAFL_payload* kafl_init()
{
    host_config_t host_config = {0};
    agent_config_t agent_config = {0};
    kAFL_payload* buffer = NULL;
    
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
    agent_config.agent_non_reload_mode = 1;
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
    // [TO-DO] - get relevant ranges (USB host stack)
    // [https://intellabs.github.io/kAFL/reference/hypercall_api.html#range-submit]
    
    // 8. submit CR3
    kAFL_hypercall(HYPERCALL_KAFL_SUBMIT_CR3, 0);
    return NULL;
}

int main() {
    int fd;
    fd = usb_open();

	if(freopen(NULL, "w", stdout) == NULL) return -1;

    fprintf(stdout, "[i] fuzzer started\n");
    kAFL_payload *payload = kafl_init(); 
    usb_init(fd, USB_SPEED_HIGH, "dummy_udc.0", "dummy_udc");
    fprintf(stdout, "[+] USB device initialized\n");
    usb_run(fd);
    
    // setup the USB device simulation
    if (setup_usb_device(fd) != 0) {
        fprintf(stderr, "Failed to set up USB device\n");
        close(fd);
        return -1;
    }

    // send a USB request (customized for fuzzing)
    if (send_usb_request(fd) != 0) {
        fprintf(stderr, "Failed to send USB request\n");
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
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
