
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <endian.h>
#include <termios.h>
#include <stdio.h> /* DEBUG */
#include "cerebrum.h"
#include "openstep.h"


static ssize_t read_sample(int serial, sample_data_t *out) {
    ssize_t rd = cbm_rx_unescape(serial, out, sizeof(*out));
    for (size_t i=0; i<SENSORS_PER_STEP; i++) {
        out->d[i] = le16toh(out->d[i]);
    }
    printf("Read sample data: %zd\n", rd);
    return rd == 6 ? 6 : -1;
}

int read_adc(int serial, sample_data_t *sd, size_t ndev) {
    printf("Acquiring data...\n");
    cbm_send_broadcast(serial, BCMD_ACQUIRE, NULL, 0);
    usleep(SAMPLE_DELAY_US);
    printf("Getting data...\n");
    tcflush(serial, TCIFLUSH);
    cbm_send_broadcast(serial, BCMD_GET_DATA, NULL, 0);

    printf("Reading data...\n");
    for (size_t i=0; i<ndev; i++) {
        if (read_sample(serial, sd+i) < 0)
            return -1;
    }
    return 0;
}
