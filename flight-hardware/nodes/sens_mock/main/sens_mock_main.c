#include "databus.h"
#include "esp_led.h"
#include "mock.h"
#include "mock_store.h"
#include "sensor_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "node/sens_mock";

void app_main(void) {
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Store *store = &mock_store;
    StatusIndicator *status_indicator = &esp_led;

    ESP_ERROR_CHECK(databus.init(NODE_ID_SENS_MOCK));
    ESP_ERROR_CHECK(store->init());
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    Sensor *sensor_mock = &mock;

    static SensorSchedule schedules[] = {
        {"mock", NULL, NULL, pdMS_TO_TICKS(1000)},
    };
    static SensorTaskContext task_contexts[sizeof(schedules) / sizeof(schedules[0])];

    schedules[0].sensor = sensor_mock;
    schedules[0].status_indicator = status_indicator;

    SemaphoreHandle_t sensor_output_mutex = xSemaphoreCreateMutex();
    if (sensor_output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        return;
    }

    for (size_t i = 0; i < sizeof(schedules) / sizeof(schedules[0]); ++i) {
        task_contexts[i] = (SensorTaskContext){
            .schedule = &schedules[i],
            .store = store,
            .sensor_output_mutex = sensor_output_mutex,
        };
        BaseType_t task_created = xTaskCreate(sensor_task, "sensor_read", 4096, &task_contexts[i], 5, NULL);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task for sensor %zu", i);
        }
    }
}
