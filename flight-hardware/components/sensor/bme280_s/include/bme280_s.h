#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t bme280_init(void);
char *bme280_read(void);

extern Sensor bme280_s;
