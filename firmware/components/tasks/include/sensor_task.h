#pragma once

#include "../../sensor/sensor.h"
#include "../../status_indicator/status_indicator.h"
#include "../../store/store.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stddef.h>

typedef struct {
    char *name;
    struct sensor *sensor;
    StatusIndicator *status_indicator;
    TickType_t read_interval;
} SensorSchedule;

typedef struct {
    const SensorSchedule *schedule;
    struct store *store;
    SemaphoreHandle_t sensor_output_mutex;
} SensorTaskContext;

void sensor_task(void *arg);
esp_err_t start_sensor_tasks(SensorSchedule *schedules, SensorTaskContext *contexts, size_t schedule_count,
                             struct store *store, StatusIndicator *status_indicator);
