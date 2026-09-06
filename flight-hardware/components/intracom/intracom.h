#pragma once

#include "databus_message.h"
#include <esp_err.h>

typedef struct Intracom {
    esp_err_t (*init)(void);
    esp_err_t (*send)(time_t time, char *msg_str);
    esp_err_t (*register_recv_callback)(uint64_t message_type, void (*callback)(struct databus_message *));
} Intracom;
