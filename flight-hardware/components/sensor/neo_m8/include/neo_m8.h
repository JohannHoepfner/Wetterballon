#pragma once

#include "../../sensor.h"

#include <esp_err.h>
#include <stdbool.h>

esp_err_t neo_m8_init(void);
char *neo_m8_read(void);
char *gps_format(double latitude, double longitude, double altitude, int hour, int minute, float second, bool valid);

extern Sensor neo_m8;
