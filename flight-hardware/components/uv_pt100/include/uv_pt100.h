#pragma once

#include <esp_err.h>

esp_err_t init_ads();
double pt100_get_temp_cels();
double get_voltage_photo(int photo_num);
