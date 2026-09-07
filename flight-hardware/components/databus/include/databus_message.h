#pragma once

#include <stdint.h>
#include <time.h>

typedef enum {
    NODE_ID_CELL_GATEWAY = 1,
    NODE_ID_RADIO_GATEWAY = 2,
    NODE_ID_SENS_ENV = 3,
    NODE_ID_SENS_MISC = 4,
    NODE_ID_SENS_MOCK = 99,
} NODE_ID;

#define DATABUS_MSG_TYPE_DATA 0
#define DATABUS_MSG_TYPE_LOG 1
#define DATABUS_MSG_TYPE_TIMESYNC 2

struct __attribute__((__packed__)) databus_message {
    uint64_t send_time;
    uint8_t type;
    uint8_t node_id;
    uint64_t msg_id;
    union {
        struct {
            char message[100];
        } data;
        struct {
            char message[100];
        } log;
        struct {
            time_t time;
        } timesync;
    };
};

int databus_message_to_send(struct databus_message *in_message, struct databus_message *out_message);
int databus_message_from_recv(struct databus_message *in_message, struct databus_message *out_message);
const char *databus_get_node_name(NODE_ID node_id);
