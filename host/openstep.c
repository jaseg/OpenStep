
#define _DEFAULT_SOURCE
#include <unistd.h>
#include <endian.h>
#include <termios.h>
#include "cerebrum.h"
#include "openstep.h"


static ssize_t read_sample(int serial, sample_data_t *out) {
    ssize_t rd = cbm_rx_unescape(serial, out, sizeof(*out));
    for (size_t i=0; i<SENSORS_PER_STEP; i++) {
        out->d[i] = le16toh(out->d[i]);
    }
    return rd == 6 ? 6 : -1;
}

int read_adc(int serial, sample_data_t *sd, size_t ndev) {
    cbm_send_broadcast(serial, BCMD_ACQUIRE, NULL, 0);
    usleep(SAMPLE_DELAY_US);
    tcflush(serial, TCIFLUSH);
    cbm_send_broadcast(serial, BCMD_GET_DATA, NULL, 0);

    for (size_t i=0; i<ndev; i++) {
        if (read_sample(serial, sd+i) < 0)
            return -1;
    }
    return 0;
}
