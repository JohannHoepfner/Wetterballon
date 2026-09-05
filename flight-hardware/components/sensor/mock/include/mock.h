#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t mock_init(void);
double mock_get_freq();

extern Sensor mock_sensor;
