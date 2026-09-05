#pragma once

#include <esp_err.h>

typedef struct Sensor Sensor;

struct Sensor {
    void *ctx;

    esp_err_t (*init)(Sensor *self);
    char *(*read)(Sensor *self);
};
