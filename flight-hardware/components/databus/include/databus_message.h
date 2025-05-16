#pragma once

#include "databus_message.h"

#include <stdint.h>
#include <time.h>

enum databus_message_type {
    databus_message_type_data = 0,
    databus_message_type_log = 1,
    databus_message_type_timesync = 2,
};

struct databus_message {
    uint64_t send_time;
    enum databus_message_type type;
    union {
        struct {
            char message[200];
        } data;
        struct {
            char message[200];
        } log;
        struct {
            time_t time;
        } time;
    };
};

int databus_message_to_bytes(struct databus_message *message, char buf[sizeof(struct databus_message)]);
int databus_message_from_bytes(char buf[sizeof(struct databus_message)], struct databus_message *message);
