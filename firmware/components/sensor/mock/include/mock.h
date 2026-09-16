#pragma once

#include "../../sensor.h"

#include <esp_err.h>

esp_err_t mock_init(void);
char *mock_read(void);
char *mock_format(double value);

extern struct sensor mock;
