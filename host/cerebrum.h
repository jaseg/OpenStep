#ifndef _CEREBRUM_H
#define _CEREBRUM_H

#include <stdint.h>
#include <unistd.h>


#define DISCOVERY_TIMEOUT  20000L /* microseconds */
#define READ_TIMEOUT      500000L /* microseconds */

#define BROADCAST_ADDR    0xFF


typedef uint64_t macaddr_t;

typedef struct {
    macaddr_t mac; /* permanent mac */
} node_info_t;

typedef struct {
    uint8_t preamble[2];
    uint8_t addr;
    uint8_t cmd;
} pkt_hdr_t;


int cbm_discover(int serial, node_info_t **inf);
int cbm_send(int serial, uint8_t addr, uint8_t cmd, void *buf, size_t len);
int cbm_send_broadcast(int serial, uint8_t cmd, void *buf, size_t len);
ssize_t cbm_rx_unescape(int serial, void *buf, size_t len);
int cbm_read_unescape(int fd, char *buf, size_t len);

#endif/*_CEREBRUM_H*/
