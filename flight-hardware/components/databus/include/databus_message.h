#pragma once

#include "databus_message.h"

#include <stdint.h>
#include <time.h>

#define DATABUS_MSG_TYPE_DAT 0
#define DATABUS_MSG_TYPE_LOG 1
#define DATABUS_MSG_TYPE_TMS 2

struct __attribute__((__packed__)) databus_message {
    uint64_t send_time;
    uint16_t type;
    union {
        struct {
            char message[220];
        } data;
        struct {
            char message[220];
        } log;
        struct {
            time_t time;
        } timesync;
    };
};

int databus_message_to_send(struct databus_message *in_message, struct databus_message *out_message);
int databus_message_from_recv(struct databus_message *in_message, struct databus_message *out_message);
