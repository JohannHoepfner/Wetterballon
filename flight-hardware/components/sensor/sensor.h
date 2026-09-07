#pragma once

#include <esp_err.h>

typedef struct Sensor Sensor;
typedef void (*sensor_callback_t)(const char *value);

struct Sensor {
    esp_err_t (*init)(void);
    char *(*read)(void);
    esp_err_t (*on_receive)(sensor_callback_t callback);
};
