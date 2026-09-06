#pragma once

#include "../../intracom.h"

#include "databus_message.h"
#include "esp_err.h"
#include <time.h>

esp_err_t databus_init(NODE_ID node_id);
esp_err_t databus_send(struct databus_message *msg);
esp_err_t databus_send_data(time_t send_time, char *msg_str);
esp_err_t databus_register_recv_callback(uint64_t message_type, void (*callback)(struct databus_message *));

extern Intracom databus;
