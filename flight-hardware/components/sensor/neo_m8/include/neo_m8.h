#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t neo_m8_init(Sensor *self);
char *neo_m8_read(Sensor *self);
char *gps_format(double latitude, double longitude, double altitude);

extern Sensor neo_m8;
