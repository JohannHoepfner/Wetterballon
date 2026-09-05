#pragma once

#include <esp_err.h>

typedef struct StatusIndicator {
    esp_err_t (*init)();
    esp_err_t (*set_status)(struct color color); // TODO: Should later just contain an enum for status
} StatusIndicator;
