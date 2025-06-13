#include "include/databus_message.h"

#include "endian.h"
#include <limits.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

int databus_message_to_send(struct databus_message *in_message, struct databus_message *out_message) {
    out_message->send_time = htobe64(in_message->send_time);

    out_message->type = htobe16(in_message->type);

    switch (in_message->type) {
    case DATABUS_MSG_TYPE_DAT:
        memcpy(out_message->data.message, in_message->data.message, sizeof(in_message->data.message));
        break;
    case DATABUS_MSG_TYPE_LOG:
        memcpy(out_message->log.message, in_message->log.message, sizeof(in_message->log.message));
        break;
    case DATABUS_MSG_TYPE_TMS: {
        out_message->timesync.time = htobe64(in_message->timesync.time);
        break;
    }
    }

    return 0;
}
int databus_message_from_recv(struct databus_message *in_message, struct databus_message *out_message) {
    out_message->send_time = be64toh(in_message->send_time);

    out_message->type = be16toh(in_message->type);

    switch (out_message->type) {
    case DATABUS_MSG_TYPE_DAT:
        memcpy(out_message->data.message, in_message->data.message, sizeof(in_message->data.message));
        out_message->data.message[sizeof(out_message->data.message) - 1] = '\0';
        break;
    case DATABUS_MSG_TYPE_LOG:
        memcpy(out_message->log.message, in_message->log.message, sizeof(in_message->log.message));
        out_message->log.message[sizeof(out_message->log.message) - 1] = '\0';
        break;
    case DATABUS_MSG_TYPE_TMS: {
        out_message->timesync.time = be64toh(in_message->timesync.time);
        break;
    }
    }

    return 0;
}
