#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t mock_init(Sensor *self);
char *mock_read(Sensor *self);
char *mock_format(double value);

extern Sensor mock;
