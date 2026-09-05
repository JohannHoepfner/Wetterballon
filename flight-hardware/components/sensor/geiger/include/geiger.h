#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t geiger_init(Sensor *self);
double geiger_get_freq(Sensor *self);

extern Sensor geiger_sensor;
