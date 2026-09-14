#pragma once

#include <stdint.h>
#include <time.h>

#define NODES                                                                                                          \
    NODE(1, CELL_GATEWAY)                                                                                              \
    NODE(2, RADIO_GATEWAY)                                                                                             \
    NODE(3, SENS_ENV)                                                                                                  \
    NODE(4, SENS_MISC)                                                                                                 \
    NODE(99, SENS_MOCK)

typedef enum {
#define NODE(no, name) NODE_ID_##name = no,
    NODES
} NODE_ID;

struct databus_message_data_content {
    char message[100];
};
struct databus_message_log_content {
    char message[100];
};
struct databus_message_timesync_content {
    time_t time;
};

#define DATABUS_MSG_TYPES                                                                                              \
    DATABUS_MSG_TYPE(0, DATA, struct { char message[100]; })                                                           \
    DATABUS_MSG_TYPE(1, LOG, struct { char message[100]; })                                                            \
    DATABUS_MSG_TYPE(2, TIMESYNC, struct { time_t time; })                                                             \
    DATABUS_MSG_TYPE(3, KILL_RADIO, struct {})

enum DATABUS_MSG_TYPE {
#define DATABUS_MSG_TYPE(no, name, ...) DATABUS_MSG_TYPE_##name = no,
    DATABUS_MSG_TYPES
#undef DATABUS_MSG_TYPE
};

struct __attribute__((__packed__)) databus_message {
    uint64_t send_time;
    uint8_t type;
    uint8_t node_id;
    uint64_t msg_id;
    union {
#define DATABUS_MSG_TYPE(no, name, private_data) private_data name##_content;
        DATABUS_MSG_TYPES
#undef DATABUS_MSG_TYPE
    };
};

int databus_message_to_send(struct databus_message *in_message, struct databus_message *out_message);
int databus_message_from_recv(struct databus_message *in_message, struct databus_message *out_message);
const char *databus_get_node_name(NODE_ID node_id);
