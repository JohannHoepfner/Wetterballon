#pragma once

#include "../../sensor/sensor.h"
#include "../../store/store.h"
#include "../../status_indicator/status_indicator.h"
#include "databus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stddef.h>

typedef struct {
    char *name;
    Sensor *sensor;
    StatusIndicator *status_indicator;
    TickType_t read_interval;
} SensorSchedule;

typedef struct {
    const SensorSchedule *schedule;
    Intracom *intracom;
    Store *store;
    SemaphoreHandle_t output_mutex;
} SensorTaskContext;

void sensor_task(void *arg);
