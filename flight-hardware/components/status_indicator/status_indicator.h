#pragma once

#include <esp_err.h>

typedef enum {
    INITIALIZING,
    UNRECOVERABLE_ERROR,
	ERROR,
	WARNING,
    OK
} Status;

typedef struct StatusIndicator StatusIndicator;

typedef struct StatusIndicator {
    void *ctx;
    esp_err_t (*init)(StatusIndicator *self);
    esp_err_t (*set_status)(StatusIndicator *self, Status status);
} StatusIndicator;
