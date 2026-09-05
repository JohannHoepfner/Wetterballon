#pragma once

#include <esp_err.h>

typedef struct Store {
    esp_err_t (*init)();
    esp_err_t (*deinit)();
    esp_err_t (*save)(struct databus_message *message);
    ssize_t (*read)(struct databus_message *out_messages, size_t start, size_t count);
} Store;
