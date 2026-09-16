#pragma once

#include "databus_message.h"
#include "esp_err.h"
#include <time.h>

typedef void (*DatabusReceiveHandler)(struct databus_message *message);

struct databus {
    esp_err_t (*init)(uint8_t node_id);
    esp_err_t (*send_data)(time_t time, char *msg_str);
    esp_err_t (*send_log)(time_t time, char *msg_str);
    esp_err_t (*send_timesync)(time_t time);
    esp_err_t (*on_receive)(uint64_t message_type, DatabusReceiveHandler handler);
    esp_err_t (*reinit)(void);
    int radio_killed;
};

esp_err_t databus_reinit(void);
esp_err_t databus_init(uint8_t node_id);
esp_err_t databus_send(struct databus_message *msg);
esp_err_t databus_send_data(time_t send_time, char *msg_str);
esp_err_t databus_send_log(time_t send_time, char *msg_str);
esp_err_t databus_send_timesync(time_t time);
esp_err_t databus_on_receive(uint64_t message_type, DatabusReceiveHandler handler);

void on_databus_timesync_default(struct databus_message *message);
void on_databus_log_default(struct databus_message *message);

extern struct databus databus;
