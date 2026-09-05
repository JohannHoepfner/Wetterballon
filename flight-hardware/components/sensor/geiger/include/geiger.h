#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t geiger_init(Sensor *self);
char *geiger_read(Sensor *self);
char *geiger_format(unsigned long long value);

extern Sensor geiger;
