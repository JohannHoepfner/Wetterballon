#pragma once

#include <esp_err.h>
#include <time.h>

typedef struct Store {
    esp_err_t (*init)();
    esp_err_t (*deinit)();
    esp_err_t (*save)(time_t time, char *msg_str);
} Store;
