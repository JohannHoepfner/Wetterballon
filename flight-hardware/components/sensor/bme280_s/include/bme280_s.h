#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t bme280_init(Sensor *self);
char *bme280_read(Sensor *self);

extern Sensor bme280_s;
