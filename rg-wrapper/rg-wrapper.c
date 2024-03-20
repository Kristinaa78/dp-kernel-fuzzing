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

#include "rg-constants.h"
#include "rg-wrapper.h"

int setup_usb_device(int fd);
int send_usb_request(int fd);

int main(int argc, char *argv[]) {
    int fd;
    fd = usb_open();
    usb_init(fd, USB_SPEED_HIGH, "dummy_udc.0", "dummy_udc");
    fprintf(stdout, "[+] USB device initialized\n");
    usb_run(fd);
    // 4. in a loop issue USB_RAW_IOCTL_EVENT_FETCH to receive events
    // from Raw Gadget and react to those depending on what kind of USB
    // gadget must be implemented
    // usb_loop(fd);

    // Setup the USB device simulation
    if (setup_usb_device(fd) != 0) {
        fprintf(stderr, "Failed to set up USB device\n");
        close(fd);
        return -1;
    }

    // Send a USB request (customized for fuzzing)
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
    return 0;
}

int send_usb_request(int fd) {
    // TO-DO
    // sending USB requests/data to the host
    return 0;
}
