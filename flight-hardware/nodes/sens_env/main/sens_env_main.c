#include "databus.h"
#include "sd_card.h"
#include "mock.h"
#include "geiger.h"
#include "bme280_s.h"
#include "pt1000.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "sens_env";

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

static void sensor_task(void *arg)
{
    const SensorTaskContext *context = (SensorTaskContext *)arg;

    while (true) {
        char *sensor_value = context->schedule->sensor->read(context->schedule->sensor);
        time_t now = time(NULL);

        xSemaphoreTake(context->output_mutex, portMAX_DELAY);

        esp_err_t err = context->store->save(now, sensor_value);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save data to SD card: %s (0x%x)",
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

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
        
    Intracom *intracom_databus = &databus;
    Store *store_sd_card = &sd_card;

    // Sensor *sensor_mock = &mock;
    // Sensor *sensor_geiger = &geiger;
    // Sensor *sensor_bme280 = &bme280_s;
    Sensor *sensor_pt1000 = &pt1000;

    ESP_ERROR_CHECK(intracom_databus->init());
    ESP_ERROR_CHECK(store_sd_card->init());
    // ESP_ERROR_CHECK(sensor_mock->init(sensor_mock));
    // ESP_ERROR_CHECK(sensor_geiger->init(sensor_geiger));
    // ESP_ERROR_CHECK(sensor_bme280->init(sensor_bme280));
    ESP_ERROR_CHECK(sensor_pt1000->init(sensor_pt1000));

    // static SensorSchedule schedules[] = {
    //     {NULL, pdMS_TO_TICKS(1000)},
    //     {NULL, pdMS_TO_TICKS(500)},
    //     {NULL, pdMS_TO_TICKS(1000)},
    //     {NULL, pdMS_TO_TICKS(1000)},
    // };
    static SensorSchedule schedules[] = {
        {NULL, pdMS_TO_TICKS(1000)},
    };
    static SensorTaskContext task_contexts[sizeof(schedules) / sizeof(schedules[0])];

    schedules[0].sensor = sensor_pt1000;

    SemaphoreHandle_t output_mutex = xSemaphoreCreateMutex();
    if (output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        return;
    }

    for (size_t i = 0; i < sizeof(schedules) / sizeof(schedules[0]); ++i) {
        task_contexts[i] = (SensorTaskContext){
            .schedule = &schedules[i],
            .intracom = intracom_databus,
            .store = store_sd_card,
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