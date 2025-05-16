#include "include/databus_message.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

/// serialization format:
/// 0-8 send time
/// 8-10 type

int databus_message_to_bytes(struct databus_message *message, char buf[sizeof(struct databus_message)]) {
    uint64_t time = message->send_time;
    for (int i = 0; i < sizeof(time); ++i) {
        buf[i] = time >> (CHAR_BIT * (sizeof(time) - (i + 1)));
    }

    buf += sizeof(time);

    uint16_t type = message->type;
    for (int i = 0; i < sizeof(type); ++i) {
        buf[i] = type >> (CHAR_BIT * (sizeof(type) - (i + 1)));
    }
    buf += sizeof(type);

    switch (message->type) {
    case databus_message_type_data:
        memcpy(buf, message->data.message, sizeof(message->data.message));
        break;
    case databus_message_type_log:
        memcpy(buf, message->log.message, sizeof(message->log.message));
        break;
    case databus_message_type_timesync: {
        uint64_t sync_time = message->time.time;
        for (int i = 0; i < sizeof(sync_time); ++i) {
            buf[i] = sync_time >> (CHAR_BIT * (sizeof(sync_time) - (i + 1)));
        }
        break;
    }
    }

    return 0;
}
int databus_message_from_bytes(char buf[sizeof(struct databus_message)], struct databus_message *message) {
    uint64_t time = 0;
    for (int i = 0; i < sizeof(time); ++i) {
        time += buf[i] << (CHAR_BIT * (sizeof(time) - (i + 1)));
    }
    buf += sizeof(time);
    message->send_time = time;

    uint16_t type = 0;
    for (int i = 0; i < sizeof(type); ++i) {
        type += buf[i] << (CHAR_BIT * (sizeof(type) - (i + 1)));
    }
    message->type = type;
    buf += sizeof(type);

    switch (message->type) {
    case databus_message_type_data:
        memcpy(message->data.message, buf, sizeof(message->data.message));
        break;
    case databus_message_type_log:
        memcpy(message->log.message, buf, sizeof(message->log.message));
        break;
    case databus_message_type_timesync: {
        uint64_t sync_time = 0;
        for (int i = 0; i < sizeof(sync_time); ++i) {
            sync_time += buf[i] << (CHAR_BIT * (sizeof(sync_time) - (i + 1)));
        }
        message->time.time = sync_time;
        break;
    }
    default:
        return 1;
    }

    return 0;
}
