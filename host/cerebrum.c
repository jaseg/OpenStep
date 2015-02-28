
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <endian.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include "cerebrum.h"

static int read_timeout(int fd, char *buf, size_t len, unsigned long timeout) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    if ((fd = select(1, &set, NULL, NULL, &((struct timeval){.tv_sec=0, .tv_usec=timeout}))) < 1)
        return fd;
    return read(fd, buf, len);
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
        htole64(mac)
    };
    write(serial, &packet, sizeof(packet));
    int rd;
    char c;
    while ((rd=read_timeout(serial, &c, 1, DISCOVERY_TIMEOUT)) == 1)
        if (c == 0xFF)
            return 1;
    return 0;
}

static int probe_recurse(node_info_t **inf, size_t *isize, macaddr_t mac, uint8_t mask, uint8_t found, int serial) {
    if (mask == 2*sizeof(macaddr_t)) {
        (*inf)[found] = (node_info_t){.mac=mac };
        if(found >= *isize) {
            *isize *= 2;
            *inf = realloc(*inf, *isize);
            if (!inf)
                return -2;
        }
    }

    for (uint8_t nibble=0; nibble<0x10; nibble++) {
        macaddr_t newmac = mac | nibble<<(4*mask);

        int res = probe(newmac, mask, found, serial);
        if (res < 0)
            return res;

        if (res == 0)
            continue;

        res = probe_recurse(inf, isize, newmac, mask++, found, serial);
        if (res<0)
            return res;

        found += res;
    }

    return found;
}

int cbm_discover(int serial, node_info_t **inf) {
    if (!inf || *inf)
        return -1;
    size_t isize = 32;
    *inf = malloc(sizeof(node_info_t)*isize);

    if (!*inf)
        return -2;

    return probe_recurse(inf, &isize, 0, 0, 0, serial);
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
        rd = read(serial, &c, 1);
        if (rd != 1)
            return rd;
        if (c == '!')
            break;
    } while(23);
    return cbm_read_unescape(serial, buf, len);
}

int cbm_read_unescape(int fd, void *buf_in, size_t len) {
    ssize_t rd;
    char *buf = (char*)buf_in;
    char *p1 = buf;
    char *p2 = buf;
    char *end = buf+len;
    while ((rd = read_timeout(fd, p2, (p1-buf), READ_TIMEOUT)) > 0 && p1 < end) {
        while (p2 < (buf+rd)) {
            if(*p2 == '\\')
                p2++;
            *p1++ = *p2++;
        }
    }
    return (rd < 0) ? rd : (p1-buf);
}

