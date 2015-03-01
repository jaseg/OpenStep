#ifndef _MQTT_H
#define _MQTT_H

#include <stdint.h>
#include <sys/sysinfo.h>
#include <MQTTClient.h>
#include "openstep.h"
#include "cerebrum.h"


#define MQTT_BROKER "tcp://c-beam.cbrp3.c-base.org:1883"
#define CLIENTID    "OpenStep"
#define T_RT        "OpenStep"
#define T_STATE     (T_RT "/state")
#define T_STEPS     (T_RT "/steps")
#define T_LEDS      (T_RT "/leds")
#define TIMEOUT     10000L /* milliseconds */


typedef struct {
    MQTTClient cl;
} mqtt_st_t;

typedef struct {
    struct sysinfo sysinfo;
    uint32_t       sp_ivl; /* approximate samples interval in microseconds */
    uint8_t        nsteps;
    uint8_t        nsens;
    uint16_t       _pad; /* 8-byte-alignment */
    macaddr_t      bus_macs[0];
} mqtt_st_msg_t;

typedef struct {
    uint8_t        nsteps;
    uint8_t        nsens;
    sample_data_t  stepdata[0];
} mqtt_step_msg_t;


int mqtt_publish_status(mqtt_st_t *st, mqtt_st_msg_t *data);
int mqtt_publish_steps(mqtt_st_t *st, mqtt_step_msg_t *data);
mqtt_st_t *mqtt_init(void);
void mqtt_close(mqtt_st_t *st);

#endif/*_MQTT_H*/
