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

#include "constants.h"
#include "fuzzer.h"

int setup_usb_device(int fd);
int send_usb_request(int fd);

int main() {
    int fd;
    fd = usb_open();

	if(freopen(NULL, "w", stdout) == NULL) return -1;

    fprintf(stdout, "[i] fuzzer started\n");
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
