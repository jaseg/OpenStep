
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "mqtt.h"


/* this function will fill in the sysinfo part */
int mqtt_publish_status(mqtt_st_t *st, mqtt_st_msg_t *data) {
    sysinfo(&data->sysinfo);
    
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = data;
    msg.payloadlen = sizeof(*data) + sizeof(uint64_t)*data->nsteps;
    msg.qos = 1; /* at-least-once */
    msg.retained = true;
    return MQTTClient_publishMessage(st->cl, T_STATE, &msg, NULL) == MQTTCLIENT_SUCCESS;
}

/* accepting number of steps, sensors per step and raw data array as arguments
 * also, please don't fumble around with the data buffer after passing it to this function */
int mqtt_publish_steps(mqtt_st_t *st, mqtt_step_msg_t *data) {
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = data;
    msg.payloadlen = sizeof(*data) + data->nsteps*data->nsens;
    msg.qos = 0; /* fire-and-forget */
    msg.retained = false;
    return MQTTClient_publishMessage(st->cl, T_STEPS, &msg, NULL) == MQTTCLIENT_SUCCESS;
}

static void mqtt_connlost(void *context, char *cause) {
    fprintf(stderr, "MQTT connection lost: %s\n", cause);
}

mqtt_st_t *mqtt_init() {
    int rc;
    mqtt_st_t *st = malloc(sizeof(*st));
    memset(st, 0, sizeof(*st));

    if (!st)
        return NULL;

    rc = MQTTClient_create(&st->cl, MQTT_BROKER, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS)
        goto error;

    MQTTClient_setCallbacks(st->cl, NULL, mqtt_connlost, NULL, NULL);

    MQTTClient_connectOptions co = MQTTClient_connectOptions_initializer;
    co.keepAliveInterval = 20;
    co.cleansession = 1;

    rc = MQTTClient_connect(st->cl, &co);
    if (rc != MQTTCLIENT_SUCCESS)
        goto error;

    return st;

error:
    if (st->cl)
        MQTTClient_destroy(&st->cl);
    free(st);
    return NULL;
}

void mqtt_close(mqtt_st_t *st) {
    if (!st) return;
    MQTTClient_disconnect(st->cl, TIMEOUT);
    MQTTClient_destroy(&st->cl);
    free(st);
}
