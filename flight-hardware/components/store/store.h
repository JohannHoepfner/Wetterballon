#pragma once

#include "databus.h"

#include <esp_err.h>

typedef struct Store {
    esp_err_t (*init)();
    esp_err_t (*deinit)();
    esp_err_t (*save)(time_t time, char *msg_str);
    ssize_t (*load)(struct databus_message *out_messages, size_t start, size_t count);
} Store;
