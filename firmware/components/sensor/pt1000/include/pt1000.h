#pragma once

#include "../../sensor.h"
#include "ads1115.h"

#include <esp_err.h>

esp_err_t init_ads(void);
double get_voltage(ads1115_t *ads, ads1115_mux_t mux, ads1115_fsr_t fsr);
esp_err_t pt1000_init(void);
char *pt1000_read(void);
char *pt1000_format(double differential_voltage, double a3_voltage);

extern struct sensor pt1000;
