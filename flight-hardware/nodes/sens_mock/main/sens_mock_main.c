#include "databus.h"
#include "log_store.h"
#include "mock.h"
#include "sensor_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "sens_mock";

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    Intracom *intracom = &databus;
    Store *store = &log_store;

    Sensor *sensor_mock = &mock;

    ESP_ERROR_CHECK(intracom->init());
    ESP_ERROR_CHECK(store->init());
    ESP_ERROR_CHECK(sensor_mock->init(sensor_mock));

    static SensorSchedule schedules[] = {
        {NULL, pdMS_TO_TICKS(1000)},
    };
    static SensorTaskContext task_contexts[sizeof(schedules) / sizeof(schedules[0])];

    schedules[0].sensor = sensor_mock;

    SemaphoreHandle_t output_mutex = xSemaphoreCreateMutex();
    if (output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        return;
    }

    for (size_t i = 0; i < sizeof(schedules) / sizeof(schedules[0]); ++i) {
        task_contexts[i] = (SensorTaskContext){
            .schedule = &schedules[i],
            .intracom = intracom,
            .store = store,
            .output_mutex = output_mutex,
        };
        BaseType_t task_created = xTaskCreate(
            sensor_task,
            "sensor_read",
            4096,
            &task_contexts[i],
            5,
            NULL);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task for sensor %zu", i);
        }
    }
}
