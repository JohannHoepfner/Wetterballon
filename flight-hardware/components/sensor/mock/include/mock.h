#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t mock_init(void);
char *mock_read(void);

extern Sensor mock;
