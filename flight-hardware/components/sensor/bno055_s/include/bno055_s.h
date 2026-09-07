#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t bno055_init(void);
char *bno055_read(void);
char *bno055_format(double value);

extern Sensor bno055_s;
