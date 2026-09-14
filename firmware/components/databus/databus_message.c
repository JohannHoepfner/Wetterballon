#include "databus_message.h"

#include "endian.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

const char *databus_get_node_name(NODE_ID node_id) {
    switch (node_id) {
    case NODE_ID_CELL_GATEWAY:
        return "cell_gateway";
    case NODE_ID_RADIO_GATEWAY:
        return "radio_gateway";
    case NODE_ID_SENS_ENV:
        return "sens_env";
    case NODE_ID_SENS_MISC:
        return "sens_misc";
    case NODE_ID_SENS_MOCK:
        return "sens_mock";
    default:
        static char unknown_node_name[32];
        snprintf(unknown_node_name, sizeof(unknown_node_name), "unknown (node_id: %d)", node_id);
        return unknown_node_name;
    }
}

int databus_message_to_send(struct databus_message *in_message, struct databus_message *out_message) {
    out_message->send_time = htobe64(in_message->send_time);
    out_message->type = in_message->type;
    out_message->node_id = in_message->node_id;
    out_message->msg_id = htobe64(in_message->msg_id);

    switch (in_message->type) {
    case DATABUS_MSG_TYPE_DATA:
        memcpy(out_message->data.message, in_message->data.message, sizeof(in_message->data.message));
        break;
    case DATABUS_MSG_TYPE_LOG:
        memcpy(out_message->log.message, in_message->log.message, sizeof(in_message->log.message));
        break;
    case DATABUS_MSG_TYPE_TIMESYNC:
        out_message->timesync.time = htobe64(in_message->timesync.time);
        break;
    }

    return 0;
}

int databus_message_from_recv(struct databus_message *in_message, struct databus_message *out_message) {
    out_message->send_time = be64toh(in_message->send_time);
    out_message->type = in_message->type;
    out_message->node_id = in_message->node_id;
    out_message->msg_id = be64toh(in_message->msg_id);

    switch (out_message->type) {
    case DATABUS_MSG_TYPE_DATA:
        memcpy(out_message->data.message, in_message->data.message, sizeof(in_message->data.message));
        out_message->data.message[sizeof(out_message->data.message) - 1] = '\0';
        break;
    case DATABUS_MSG_TYPE_LOG:
        memcpy(out_message->log.message, in_message->log.message, sizeof(in_message->log.message));
        out_message->log.message[sizeof(out_message->log.message) - 1] = '\0';
        break;
    case DATABUS_MSG_TYPE_TIMESYNC:
        out_message->timesync.time = be64toh(in_message->timesync.time);
        break;
    }

    return 0;
}
