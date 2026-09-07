#pragma once

#include "databus_message.h"
#include "esp_err.h"
#include <time.h>

typedef struct Databus Databus;
typedef void (*DatabusReceiveHandler)(struct databus_message *message);

struct Databus {
	esp_err_t (*init)(uint8_t node_id);
	esp_err_t (*send)(time_t time, char *msg_str);
	esp_err_t (*on_receive)(uint64_t message_type, DatabusReceiveHandler handler);
};

esp_err_t databus_init(uint8_t node_id);
esp_err_t databus_send(struct databus_message *msg);
esp_err_t databus_send_data(time_t send_time, char *msg_str);
esp_err_t databus_on_receive(uint64_t message_type, DatabusReceiveHandler handler);

extern Databus databus;
