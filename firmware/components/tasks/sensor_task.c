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
void
sensor_task(void *arg) {
    const SensorTaskContext *context = (SensorTaskContext *)arg;

    if (context == NULL || context->schedule == NULL) {
        ESP_LOGE(TAG, "Invalid sensor task context");
        vTaskDelete(NULL);
        return;
    }

    if (context->schedule->sensor == NULL) {
        ESP_LOGE(TAG, "Sensor '%s': No adapter defined, stopping sensor task", context->schedule->name);
        if (context->schedule->status_indicator != NULL) {
            context->schedule->status_indicator->set_status(context->schedule->status_indicator,
                                                            STATUS_INDICATOR_SENSOR_ERROR);
        }
        vTaskDelete(NULL);
        return;
    }

    if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
        context->schedule->status_indicator->set_status(context->schedule->status_indicator,
                                                        STATUS_INDICATOR_INITIALIZING);
    }

    // Initialize the sensor
    ESP_LOGI(TAG, "Sensor '%s': STATUS_INDICATOR_INITIALIZING...", context->schedule->name);
    esp_err_t err = context->schedule->sensor->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor '%s': Failed to initialize sensor: %s (0x%x)", context->schedule->name,
                 esp_err_to_name(err), err);
        if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
            context->schedule->status_indicator->set_status(context->schedule->status_indicator,
                                                            STATUS_INDICATOR_SENSOR_ERROR);
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Sensor '%s': Initialized successfully", context->schedule->name);

    // Continuously read sensor data, store it and send via databus
    int save_fail_count = 0;
    int reset_save_fail_count = 5; // Number of consecutive save failures before reinitializing the store
    int send_fail_count = 0;
    int reset_send_fail_count = 5; // Number of consecutive send failures before reinitializing the databus
    while (true) {
        // Read the sensor data
        char *sensor_value = context->schedule->sensor->read();
        if (sensor_value == NULL) {
            ESP_LOGE(TAG, "Sensor '%s': Failed to read sensor data", context->schedule->name);
            if (context->schedule->status_indicator != NULL &&
                context->schedule->status_indicator->set_status != NULL) {
                context->schedule->status_indicator->set_status(context->schedule->status_indicator,
                                                                STATUS_INDICATOR_ERROR);
            }
            vTaskDelay(context->schedule->read_interval);
            continue;
        }

        time_t now = time(NULL);

        xSemaphoreTake(context->sensor_output_mutex, portMAX_DELAY);

        struct store *store = context->store;

        esp_err_t store_err = store->save(store, now, sensor_value);
        if (store_err != ESP_OK) {
            save_fail_count++;
            ESP_LOGE(TAG, "Sensor '%s': Failed to save data to store: %s (0x%x)", context->schedule->name,
                     esp_err_to_name(store_err), store_err);
        } else {
            reset_save_fail_count = 5;
            ESP_LOGD(TAG, "Sensor '%s': Data saved to store successfully", context->schedule->name);
        }

        esp_err_t databus_err = databus.send_data(now, sensor_value);
        if (databus_err != ESP_OK) {
            send_fail_count++;
            ESP_LOGE(TAG, "Sensor '%s': Failed to send data via databus: %s (0x%x)", context->schedule->name,
                     esp_err_to_name(databus_err), databus_err);
        } else {
            reset_send_fail_count = 5;
            ESP_LOGD(TAG, "Sensor '%s': Data sent via databus successfully", context->schedule->name);
        }

        if (context->schedule->status_indicator != NULL && context->schedule->status_indicator->set_status != NULL) {
            context->schedule->status_indicator->set_status(
                context->schedule->status_indicator,
                store_err == ESP_OK && databus_err == ESP_OK ? STATUS_INDICATOR_OK : STATUS_INDICATOR_ERROR);
        }

        if (save_fail_count > reset_save_fail_count) {
            ESP_LOGW(TAG, "Sensor '%s': Too many save failures, reinitializing store", context->schedule->name);
            store->reinit(store);
            save_fail_count = 0;
            reset_save_fail_count += 2 * reset_save_fail_count;
        } else if (send_fail_count > reset_send_fail_count) {
            ESP_LOGW(TAG, "Sensor '%s': Too many send failures, reinitializing databus", context->schedule->name);
            databus.reinit();
            send_fail_count = 0;
            reset_send_fail_count += 2 * reset_send_fail_count;
        }

        xSemaphoreGive(context->sensor_output_mutex);
        vTaskDelay(context->schedule->read_interval);
    }
}

esp_err_t
start_sensor_tasks(struct sensor_schedule *schedules, SensorTaskContext *contexts, size_t schedule_count,
                   struct store *store, struct status_indicator *status_indicator) {
    SemaphoreHandle_t sensor_output_mutex = xSemaphoreCreateMutex();
    if (sensor_output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
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
            status_indicator->set_status(status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}
