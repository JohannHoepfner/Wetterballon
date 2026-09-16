#pragma once

#include "../../sensor/sensor.h"
#include "../../status_indicator/status_indicator.h"
#include "../../store/store.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <stddef.h>

struct sensor_schedule {
    char *name;
    struct sensor *sensor;
    struct status_indicator *status_indicator;
    TickType_t read_interval;
};

typedef struct {
    const struct sensor_schedule *schedule;
    struct store *store;
    SemaphoreHandle_t sensor_output_mutex;
} SensorTaskContext;

void sensor_task(void *arg);
esp_err_t start_sensor_tasks(struct sensor_schedule *schedules, SensorTaskContext *contexts, size_t schedule_count,
                             struct store *store, struct status_indicator *status_indicator);
