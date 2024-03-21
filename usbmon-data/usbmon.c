/**
 * usbmon.c - program that collects the raw USB data that is traced by usbmon kernel module
 * in a binary format. it interacts with a device file (e.g., /dev/usbmon3). usbmon kernel
 * module needs to be insmoded beforehand (with 'sudo modprobe usbmon')
 *
 * program needs to be run with root privileges.
 *
 * Author:   Kristina Hrebenarova
 * Created:  03.2024
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>

#define SETUP_LEN           8
#define DATA_LEN            1024
#define MAX_EVENTS          32
// /dev/usbmonN - N = the USB bus number
#define CHAR_DEVICE         "/dev/usbmon0"

#define MON_IOC_MAGIC		0x92
#define MON_IOCQ_URB_LEN	_IO(MON_IOC_MAGIC, 1)
#define MON_IOCG_STATS  	_IOR(MON_IOC_MAGIC, 3, struct mon_bin_stats)
#define MON_IOCT_RING_SIZE  _IO(MON_IOC_MAGIC, 4)
#define MON_IOCQ_RING_SIZE  _IO(MON_IOC_MAGIC, 5)
#define MON_IOCX_GET		_IOW(MON_IOC_MAGIC, 6, struct mon_get_arg)      // copies 48B to hdr area
#define MON_IOCX_MFETCH     _IOWR(MON_IOC_MAGIC, 7, struct mon_mfetch_arg)
#define MON_IOCX_GETX       _IOW(MON_IOC_MAGIC, 10, struct mon_get_arg)     // copies 64B to hdr area

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef int64_t s64;
typedef int32_t s32;

struct mon_mfetch_arg {
      uint32_t *offvec;       /* offsets of events fetched */
      uint32_t nfetch;        /* number of events to fetch */
      uint32_t nflush;        /* number of events to flush */
};

struct mon_get_arg {
      struct usbmon_packet *hdr;
      void *data;
      size_t alloc;           /* length of data (can be zero) */
};

struct mon_bin_stats {
      u32 queued;             /* number of events currently queued in the buffer */
      u32 dropped;            /* number of events lost since last call */
};

// usbmon_packet structure
// from: [https://docs.kernel.org/usb/usbmon.html#raw-binary-format-and-api]
struct usbmon_packet {
    u64 id;                   /*  0: URB ID - from submission to callback */
    unsigned char type;       /*  8: Same as text; extensible. */
    unsigned char xfer_type;  /*     ISO (0), Intr, Control, Bulk (3) */
    unsigned char epnum;      /*     Endpoint number and transfer direction */
    unsigned char devnum;     /*     Device address */
    u16 busnum;               /* 12: Bus number */
    char flag_setup;          /* 14: Same as text */
    char flag_data;           /* 15: Same as text; Binary zero is OK. */
    s64 ts_sec;               /* 16: gettimeofday */
    s32 ts_usec;              /* 24: gettimeofday */
    int status;               /* 28: */
    unsigned int length;      /* 32: Length of data (submitted or actual) */
    unsigned int len_cap;     /* 36: Delivered length */
    union {                   /* 40: */
        unsigned char setup[SETUP_LEN]; /* Only for Control S-type */
        struct iso_rec {            /* Only for ISO */
            int error_count;
            int numdesc;
        } iso;
    } s;
    int interval;             /* 48: Only for Interrupt and ISO */
    int start_frame;          /* 52: For ISO */
    unsigned int xfer_flags;  /* 56: copy of URB's transfer_flags */
    unsigned int ndesc;       /* 60: Actual number of ISO descriptors */
};                            /* 64 total length */

void process_packet(struct usbmon_packet *hdr, void *data) {
    fprintf(stdout, "ID: %p, TYPE: 0x%02x, TRANSFER TYPE: 0x%02x, ENDPOINT: 0x%02x, DIR: 0x%02x, "
            "DEVICE: 0x%02x, BUS: 0x%02x, DATA: 0x%02x, STATUS: 0x%08x, URB LEN: 0x%08x, "
            "DATA LENGTH: 0x%08x, TRANSFER FLAGS: 0x%08x\n",
            (void *)hdr->id,
            hdr->type,
            hdr->xfer_type,
            hdr->epnum,
            hdr->epnum & USB_DIR_IN ? 1 : 0,
            hdr->devnum,
            hdr->busnum,
            hdr->flag_data,
            hdr->status,
            hdr->length,
            hdr->len_cap,
            hdr->xfer_flags);
    fprintf(stdout, "DATA: ");
    for (int i = 0; i < hdr->len_cap; i++)
      fprintf(stdout, "0x%02x ", ((unsigned char *)data)[i]);
    fprintf(stdout, "\n");
}

int main() {
    int fd, nflush = 3, N = 3,  buffer_size = 0;
    void *buffer;
    struct mon_mfetch_arg fetch = { 0 };

    // open usbmon character device for reading
    fd = open(CHAR_DEVICE, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[-] unable to open %s\n", CHAR_DEVICE); 
        perror("[i] error: ");    
        return 1;
    }

    // query the current size of the buffer in bytes:
    buffer_size = ioctl(fd, MON_IOCQ_RING_SIZE);
    if (buffer_size <= 0) {
        fprintf(stderr, "[-] MON_IOCQ_RING_SIZE failed with %d\n", buffer_size); 
        perror("[i] error: "); 
        close(fd);
        return 1;
    }
    fprintf(stdout, "[i] current buffer size: %d\n", buffer_size);

    // map buffer memory
    buffer = mmap(NULL, buffer_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        fprintf(stderr, "[-] mmap() failed\n"); 
        perror("[i] error: ");
        close(fd);
        return 1;
    }

    // allocate memory for events offsets 
    fetch.offvec = (uint32_t*)malloc(MAX_EVENTS * sizeof(uint32_t));
    if (!fetch.offvec) {
        fprintf(stderr, "[-] malloc for fetch.offvec failed");
        perror("[i] error: ");
        munmap(buffer, buffer_size);
        close(fd);
        return -1;
    }

    //  N = buffer_size / sizeof(struct usbmon_packet);
    //  nflush = N;

    for (;;) {
        fetch.nfetch = N;
        fetch.nflush = nflush;
        ioctl(fd, MON_IOCX_MFETCH, &fetch);
        nflush = fetch.nfetch;
        // fprintf(stdout, "[i] nfetch=%d nflush=%d\n", fetch.nfetch, fetch.nflush); 

        for (unsigned int i = 0; i < nflush; i++) {
            // fprintf(stdout, "[i] offset %u, %u\n", i, fetch.offvec[i]);
            struct usbmon_packet* hdr = (struct usbmon_packet*)((char*)buffer + fetch.offvec[i]);
            void *data = (void*)((char*)buffer + fetch.offvec[i] + 64);
            if (hdr->len_cap) process_packet(hdr, data);
        }
    }

    close(fd);
    return 0;
}
