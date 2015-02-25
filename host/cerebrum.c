
#define _DEFAULT_SOURCE
#include <endian.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>


#define DISCOVERY_TIMEOUT 10000 /* microseconds */
#define READ_TIMEOUT      500000 /* microseconds */

#define BROADCAST_ADDR    0xFF


typedef struct {
    uint8_t tid; /* temporary bus id */
    uint64_t mac; /* permanent mac */
} node_info_t;

typedef struct __attribute__((__packed__)) {
    uint8_t preamble[2];
    uint8_t addr;
    uint8_t cmd;
} pkt_t;


static int read_timeout(int fd, char *buf, size_t len, unsigned long timeout) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    if ((fd = select(1, &set, NULL, NULL, &((struct timeval){.tv_sec=0, .tv_usec=timeout}))) < 1)
        return fd;
    return read(fd, out, len);
}


static int probe(uint64_t mac, uint8_t mask, uint8_t addr, int serial) {
    tcflush(serial, TCIFLUSH);
    struct __attribute__((__packed__)) {
        uint8_t preamble[2];
        uint8_t addr;
        uint8_t mask;
        uint8_t new_addr;
        uint64_t mac;
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

static int probe_recurse(node_info_t **inf, size_t *isize, uint64_t mac, uint8_t mask, uint8_t found, int serial) {
    if (mask == 2*sizeof(uint64_t)) {
        (*inf)[found] = (node_info_t){ .tid=found, .mac=mac };
        if(found >= *isize) {
            *isize *= 2;
            *inf = realloc(*inf, *isize);
            if (!inf)
                return -2;
        }
    }

    for (uint8_t nibble=0; nibble<0x10; nibble++) {
        uint64_t newmac = mac | nibble<<(4*mask);

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

int cbm_discover(node_info_t **inf, int serial) {
    if (!inf || *inf)
        return -1;
    size_t isize = 32;
    *inf = malloc(sizeof(node_info_t)*isize);

    if (!*inf)
        return -2;

    return probe_recurse(inf, &isize, 0, 0, 0, serial);
}

int cbm_send(int serial, uint8_t addr, uint8_t cmd, void *buf, size_t len) {
    write(serial, &((pkt_hdr_t){{'\\', '?'}, addr, cmd}), sizeof(hdr));
    write(serial, buf, buf, len);
}

int cbm_send_broadcast(int serial, uint8_t cmd, void *buf, size_t len) {
    cbm_send(serial, BROADCAST_ADDR, cmd, buf, len);
}

ssize_t cbm_rx_unescape(void *buf, size_t len) {
    ssize_t rd = read(serial, &c, 1);

    samples = [read_sample(s, i) for i in range(n)]
def read_sample(s, i):
    a,b,c = struct.unpack('<HHH', s.ser.rx_unescape(6))
	def rx_unescape(self, n):
		self.flushInput()
		self.flushInput()
		r = b'$'
		while r != b'!':
			r = self.read(1)
		return self.read_unescape(n)
}

int cbm_read_unescape(int fd, void *buf, size_t len) {
    ssize_t rd;
    while ((rd = read_timeout(fd, buf, len, READ_TIMEOUT)) > 0 && n < len) {
    }
}

