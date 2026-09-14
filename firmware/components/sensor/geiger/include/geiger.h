#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t geiger_init(void);
char *geiger_read(void);
char *geiger_format(unsigned long long value);

extern Sensor geiger;
