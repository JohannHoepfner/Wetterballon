#pragma once

#include "esp_err.h"
#include <time.h>

enum databus_message_type {
    databus_message_type_data,
    databus_message_type_log,
    databus_message_type_timesync,
};

struct databus_message {
    enum databus_message_type type;
    union {
        struct databus_message_data {
            char message[200];
        } data;
        struct databus_message_log {
            char message[200];
        } log;
        struct databus_message_timesync {
            time_t time;
        } time;
    };
};

esp_err_t databus_wifi_init(void);
esp_err_t databus_init(void);

esp_err_t databus_send(struct databus_message);
esp_err_t databus_send_timesync(time_t time);
esp_err_t databus_send_data(char *msg_str);
esp_err_t databus_send_log(char *msg_str);

esp_err_t databus_register_recv_callback(enum databus_message_type type, void (*callback)(struct databus_message *));
esp_err_t databus_parse_message(char *msg_buf, struct databus_message *out_msg);
