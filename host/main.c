
#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "cerebrum.h"
#include "openstep.h"
#include "mqtt.h"


#define SAMPLES_PER_SECOND 10
/* must be a multiple of two */
#define SEND_STATUS_CYCLES 20


int main(argc, argv)
    int argc;
    char **argv;
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s DEV\nwhere DEV is the serial device to use.\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Can't open %s: %s\n", argv[1], strerror(errno));
        return 2;
    }

    if (isatty(fd) != 1) {
        fprintf(stderr, "Not a TTY: %s\n", argv[1]);
        goto error;
    }

    struct termios tio;
    memset(&tio, 0, sizeof tio);
    if (tcgetattr(fd, &tio) != 0) {
        fprintf(stderr, "Error reading tty properties\n");
        goto error;
    }
    cfsetospeed(&tio, B115200);
    cfsetispeed(&tio, B115200);
    tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tio.c_iflag &= ~IGNBRK;         /* disable break processing */
    tio.c_lflag = 0;                /* no signaling chars, no echo, no canonical processing */
    tio.c_oflag = 0;                /* no remapping, no delays */
    tio.c_cc[VMIN]  = 1;            /* read doesn't block */
    tio.c_cc[VTIME] = 50;           /* 5.0 seconds read timeout */
    tio.c_iflag &= ~(IXON | IXOFF | IXANY); /* shut off xon/xoff ctrl */
    tio.c_cflag |= (CLOCAL | CREAD);        /* ignore modem controls, */
    tio.c_cflag &= ~(PARENB | PARODD);      /* shut off parity */
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        fprintf(stderr, "Error setting tty properties\n");
        goto error;
    }

    printf("TTY opened.\n");

    node_info_t *inf = NULL;
    ssize_t ndev;
    if ((ndev=cbm_discover(fd, &inf)) < 0) {
        fprintf(stderr, "Couldn't enumerate bus\n");
        goto error;
    }
    if (ndev == 0) {
        fprintf(stderr, "Found absolutely no devices on bus\n");
        goto error;
    }
    printf("Found %zd devices on bus.\n", ndev);

    mqtt_step_msg_t *msg = malloc(sizeof(mqtt_step_msg_t) + sizeof(sample_data_t)*ndev);
    if (!msg) {
        fprintf(stderr, "Cannot allocate sample memory\n");
        goto error;
    }
    msg->nsteps = ndev;
    msg->nsens = SENSORS_PER_STEP;

    mqtt_st_msg_t *stmsg = malloc(sizeof(mqtt_st_msg_t) + sizeof(macaddr_t)*ndev);
    stmsg->nsteps = ndev;
    stmsg->nsens = SENSORS_PER_STEP;
    memcpy(stmsg->bus_macs, inf, sizeof(*inf)*ndev);

    mqtt_st_t *mqtt_st = mqtt_init();
    if (!mqtt_st) {
        fprintf(stderr, "Cannot initialize MQTT client\n");
        goto error;
    }

    unsigned int cycles = 0;
    long time_acc_us = 0;
    long delay_time = 0;
    struct timespec tvs[2];
    clock_gettime(CLOCK_REALTIME, &tvs[1]);
    while (23) {
        if (read_adc(fd, msg->stepdata, ndev) < 0) {
            fprintf(stderr, "Couldn't read sample data\n");
            usleep(500000L);
            continue;
//            goto error;
        }
        if (mqtt_publish_steps(mqtt_st, msg) < 0) {
            fprintf(stderr, "Couldn't publish sample data via MQTT\n");
            goto error;
        }

        usleep(delay_time);

        clock_gettime(CLOCK_REALTIME, &tvs[cycles&1]);
        long sign = (cycles&1) ? -1 : 1;
        long last_cycle_us = sign*(tvs[0].tv_sec - tvs[1].tv_sec)*1000000 +
                             sign*(tvs[0].tv_nsec - tvs[1].tv_nsec)/1000;

        delay_time += (1000000L/SAMPLES_PER_SECOND) - last_cycle_us;
        if (delay_time < 0)
            delay_time = 0;

        time_acc_us += last_cycle_us;
        cycles++;
        if (cycles == SEND_STATUS_CYCLES) {
            stmsg->sp_ivl = time_acc_us/SEND_STATUS_CYCLES;
            time_acc_us = 0;
            if (mqtt_publish_status(mqtt_st, stmsg) < 0) {
                fprintf(stderr, "Couldn't publish status via MQTT\n");
                goto error;
            }
        }
    }

error:
    mqtt_close(mqtt_st);
    close(fd);
    return 1;
}
