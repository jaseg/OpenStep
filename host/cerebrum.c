
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <endian.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> /* DEBUG */
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include "cerebrum.h"

static int read_timeout(int fd, char *buf, size_t len, unsigned long timeout) {
    size_t rd;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    printf("Waiting to read %zd...", len);
    struct timeval to_val = {.tv_sec=0, .tv_usec=timeout};
    if ((rd = select(fd+1, &set, NULL, NULL, &to_val)) < 1) {
        printf("error...\n");
        return rd;
    }
    printf("reading...\n");
    return read(fd, buf, len);
}

void hexDump (void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

static int probe(macaddr_t mac, uint8_t mask, uint8_t addr, int serial) {
    tcflush(serial, TCIFLUSH);
    struct __attribute__((__packed__)) {
        uint8_t preamble[2];
        uint8_t addr;
        uint8_t mask;
        uint8_t new_addr;
        macaddr_t mac;
    } packet = {
        .preamble = {'\\', '?'}, /* generic preamble */
        .addr = 0xCC, /* discovery address */
        mask,
        addr,
        htobe64(mac)
    };

    printf("Sending probe: ");
    hexDump(&packet, sizeof(packet));
    write(serial, &packet, sizeof(packet));
    int rd;
    unsigned char c;
    printf("Awaiting reply... ");
    while ((rd=read_timeout(serial, (char *)&c, 1, DISCOVERY_TIMEOUT)) == 1) {
        printf("received %hhx\n", c);
        if (c == 0xFF){
            printf("Got reply.\n");
            return 1;
        }
    }
    printf("No reply.\n");
    return 0;
}

static int probe_recurse(node_info_t **inf, size_t *isize, macaddr_t mac, uint8_t mask, uint8_t found, int serial) {
    if (mask == 2*sizeof(macaddr_t)) {
        printf("Reached leaf node\n");
        (*inf)[found] = (node_info_t){.mac=mac };
        if(found >= *isize) {
            *isize *= 2;
            printf("Reallocating inf to %zu\n", *isize);
            *inf = realloc(*inf, *isize);
            if (!inf)
                return -2;
        }
        return 1;
    }

    printf("Probing %llu/%u\n", mac, mask);
    for (uint8_t nibble=0; nibble<0x10; nibble++) {
        macaddr_t newmac = mac | (uint64_t)nibble<<(4*(16-mask));

        int res = probe(newmac, mask, found, serial);
        if (res < 0)
            return res;

        if (res == 0)
            continue;

        res = probe_recurse(inf, isize, newmac, mask+1, found, serial);
        if (res<0)
            return res;

        found += res;
    }

    return found;
}

int cbm_discover(int serial, node_info_t **inf) {
    if (!inf || *inf)
        return -1;
    size_t isize = 64;
    *inf = malloc(sizeof(node_info_t)*isize);

    if (!*inf)
        return -2;

    printf("Beginning probing...\n");
    return probe_recurse(inf, &isize, 0, 1, 0, serial);
}

int cbm_send(int serial, uint8_t addr, uint8_t cmd, void *buf, size_t len) {
    ssize_t wd;
    tcflush(serial, TCIFLUSH);
    pkt_hdr_t hdr = {{'\\', '?'}, addr, cmd};
    wd = write(serial, &hdr, sizeof(hdr));
    if (wd != sizeof(hdr))
        return -1;
    wd = write(serial, buf, len);
    if (wd != len)
        return -1;
    return 0;
}

int cbm_send_broadcast(int serial, uint8_t cmd, void *buf, size_t len) {
    tcflush(serial, TCIFLUSH);
    return cbm_send(serial, BROADCAST_ADDR, cmd, buf, len);
}

ssize_t cbm_rx_unescape(int serial, void *buf, size_t len) {
    char c;
    ssize_t rd;
    do {
        rd = read_timeout(serial, &c, 1, READ_TIMEOUT);
        printf("Finding frame, read %zd %c %hhx\n", rd, c, c);
        if (rd != 1)
            return rd;
        if (c == '!')
            break;
    } while(23);
    return cbm_read_unescape(serial, buf, len);
}

int cbm_read_unescape(int fd, char *buf, size_t len) {
    int state = 0;
    char *end = buf+len;
    ssize_t rd;
    printf("Beginning unescaping of %zu bytes.\n", len);
    while ((rd = read_timeout(fd, buf, 1, READ_TIMEOUT)) == 1 && buf < end) {
        if (!state && *buf == '\\') {
            state = 1;
        } else {
            state = 0;
            buf++;
        }
    }
    return (rd < 0) ? rd : len-(buf-end);
}

