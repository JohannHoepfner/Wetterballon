#pragma once

#include <esp_err.h>

typedef struct Sensor Sensor;

struct Sensor {
    esp_err_t (*init)(void);
    char *(*read)(void);
};
