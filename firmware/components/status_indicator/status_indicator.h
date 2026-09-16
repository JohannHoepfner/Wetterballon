#pragma once

#include <esp_err.h>

typedef enum {
    STATUS_INDICATOR_INITIALIZING,
    STATUS_INDICATOR_SD_CARD_ERROR,
    STATUS_INDICATOR_SENSOR_ERROR,
    STATUS_INDICATOR_INTERCOM_ERROR,
    STATUS_INDICATOR_ERROR,
    STATUS_INDICATOR_WARNING,
    STATUS_INDICATOR_OK
} Status;

struct status_indicator {
    void *ctx;
    esp_err_t (*init)(struct status_indicator *self);
    esp_err_t (*set_status)(struct status_indicator *self, Status status);
};
