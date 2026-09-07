#pragma once

#include <esp_err.h>

typedef enum {
    STATUS_INDICATOR_INITIALIZING,
    STATUS_INDICATOR_SD_CARD_ERROR,
    STATUS_INDICATOR_SENSOR_ERROR,
	STATUS_INDICATOR_ERROR,
	STATUS_INDICATOR_WARNING,
    STATUS_INDICATOR_OK
} Status;

typedef struct StatusIndicator StatusIndicator;

typedef struct StatusIndicator {
    void *ctx;
    esp_err_t (*init)(StatusIndicator *self);
    esp_err_t (*set_status)(StatusIndicator *self, Status status);
} StatusIndicator;
