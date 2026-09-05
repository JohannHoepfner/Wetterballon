#pragma once

#include "../../sensor/sensor.h"
#include "databus.h"
#include "log_store.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stddef.h>

typedef struct {
    Sensor *sensor;
    TickType_t read_interval;
} SensorSchedule;

typedef struct {
    const SensorSchedule *schedule;
    Intracom *intracom;
    Store *store;
    SemaphoreHandle_t output_mutex;
} SensorTaskContext;

void sensor_task(void *arg);
