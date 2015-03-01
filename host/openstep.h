#ifndef _OPENSTEP_H
#define _OPENSTEP_H

#include <stdint.h>


#define BCMD_GET_DATA         254
#define BCMD_ACQUIRE          255 /* usually followed by a break of a few milliseconds */

#define SAMPLE_DELAY_US       40000

#define SENSORS_PER_STEP      3


typedef struct {
    uint16_t d[SENSORS_PER_STEP];
} sample_data_t;


int read_adc(int serial, sample_data_t *sd, size_t ndev);

#endif/*_OPENSTEP_H*/
