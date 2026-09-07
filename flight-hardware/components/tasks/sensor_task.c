#include "sensor_task.h"
#include "databus.h"

#include <freertos/task.h>

#include <esp_err.h>
#include <esp_log.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "tasks/sensor_task";

// TODO: esp_err_t instead of void
void sensor_task(void *arg) {
    const SensorTaskContext *context = (SensorTaskContext *)arg;

    if (context == NULL || context->schedule == NULL) {
        ESP_LOGE(TAG, "Invalid sensor task context");
        vTaskDelete(NULL);
        return;
    }

    if (context->schedule->sensor == NULL) {
        ESP_LOGE(TAG, "Sensor '%s': No adapter defined, stopping sensor task", context->schedule->name);
        if (context->schedule->status_indicator != NULL) {
            context->schedule->status_indicator->set_status(context->schedule->status_indicator, UNRECOVERABLE_ERROR);
        }
        vTaskDelete(NULL);
        return;
    }

    if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
        context->schedule->status_indicator->set_status(context->schedule->status_indicator, INITIALIZING);
    }

    // Initialize the sensor
    ESP_LOGI(TAG, "Sensor '%s': Initializing...", context->schedule->name);
    esp_err_t err = context->schedule->sensor->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor '%s': Failed to initialize sensor: %s (0x%x)", context->schedule->name,
                 esp_err_to_name(err), err);
        if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
            context->schedule->status_indicator->set_status(context->schedule->status_indicator, UNRECOVERABLE_ERROR);
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Sensor '%s': Initialized successfully", context->schedule->name);

    // Continuously read sensor data, store it and send via databus
    while (true) {
        // Read the sensor data
        char *sensor_value = context->schedule->sensor->read();
        if (sensor_value == NULL) {
            ESP_LOGE(TAG, "Sensor '%s': Failed to read sensor data", context->schedule->name);
            if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
                context->schedule->status_indicator->set_status(context->schedule->status_indicator, ERROR);
            }
            vTaskDelay(context->schedule->read_interval);
            continue;
        }

        time_t now = time(NULL);

        xSemaphoreTake(context->sensor_output_mutex, portMAX_DELAY);

        esp_err_t err = context->store->save(now, sensor_value);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Sensor '%s': Failed to save data to store: %s (0x%x)", context->schedule->name,
                     esp_err_to_name(err), err);
            if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
                context->schedule->status_indicator->set_status(context->schedule->status_indicator, ERROR);
            }
        } else {
            err = databus.send_data(now, sensor_value);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Sensor '%s': Failed to send data via databus: %s (0x%x)", context->schedule->name,
                         esp_err_to_name(err), err);
                if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
                    context->schedule->status_indicator->set_status(context->schedule->status_indicator, ERROR);
                }
            } else {
                if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
                    context->schedule->status_indicator->set_status(context->schedule->status_indicator, OK);
                }
            }
        }

        xSemaphoreGive(context->sensor_output_mutex);
        vTaskDelay(context->schedule->read_interval);
    }
}

esp_err_t start_sensor_tasks(SensorSchedule *schedules, SensorTaskContext *contexts, size_t schedule_count,
                             Store *store, StatusIndicator *status_indicator) {
    SemaphoreHandle_t sensor_output_mutex = xSemaphoreCreateMutex();
    if (sensor_output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        status_indicator->set_status(status_indicator, UNRECOVERABLE_ERROR);
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < schedule_count; ++i) {
        contexts[i] = (SensorTaskContext){
            .schedule = &schedules[i],
            .store = store,
            .sensor_output_mutex = sensor_output_mutex,
        };

        BaseType_t task_created = xTaskCreate(sensor_task, "sensor_read", 4096, &contexts[i], 5, NULL);

        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task for sensor %zu", i);
            status_indicator->set_status(status_indicator, UNRECOVERABLE_ERROR);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
