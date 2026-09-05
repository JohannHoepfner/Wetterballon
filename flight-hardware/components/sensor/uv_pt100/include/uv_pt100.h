#pragma once

#include "../../sensor.h"
#include "../ads1115.h"

#include <esp_err.h>

typedef enum {
	UV_SENSOR,
	PT100_SENSOR
} SensorType;

typedef struct UV_PT100_Sensor {
	SensorType type;
	int photo_num;
} UV_PT100_Sensor;

esp_err_t init_ads(Sensor *self);
double get_photo_volt_and_temp(Sensor *self);

double pt100_get_temp_cels();
double pt100_wheatstone_volt_to_ohms(double u_v);
double pt100_ohm_to_cels(double r_ohm);
double get_voltage_photo(int photo_num);
double get_voltage(ads1115_t *ads, ads1115_mux_t mux, ads1115_fsr_t fsr);

extern Sensor uv_pt100_sensor;
