#pragma once

#include <esp_err.h>

typedef enum {
	ERROR,
	WARNING,
    OK
} Status;

typedef struct StatusIndicator {
    esp_err_t (*init)();
    esp_err_t (*set_status)(Status status);
} StatusIndicator;
