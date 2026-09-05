#include "sensor_task.h"

#include <freertos/task.h>

#include <esp_err.h>
#include <esp_log.h>
#include <stdbool.h>
#include <time.h>

static const char *TAG = "sensor_task";

void sensor_task(void *arg)
{
    const SensorTaskContext *context = (SensorTaskContext *)arg;

    while (true) {
        char *sensor_value = context->schedule->sensor->read(context->schedule->sensor);
        time_t now = time(NULL);

        xSemaphoreTake(context->output_mutex, portMAX_DELAY);

        esp_err_t err = context->store->save(now, sensor_value);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save data to store: %s (0x%x)",
                     esp_err_to_name(err), err);
        } else {
            err = context->intracom->send_data(now, sensor_value);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send data via intracom: %s (0x%x)",
                         esp_err_to_name(err), err);
            }
        }

        xSemaphoreGive(context->output_mutex);
        vTaskDelay(context->schedule->read_interval);
    }
}
