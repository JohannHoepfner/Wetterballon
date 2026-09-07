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

Store *store = &mock_store;
StatusIndicator *status_indicator = &esp_led;
Sensor *sensor_mock = &mock;

static void on_databus_data(struct databus_message *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid databus message");
        return;
    }

    if (store == NULL) {
        ESP_LOGE(TAG, "Store is not initialized");
        return;
    }

    time_t send_time = message->send_time;
    char *msg_str = message->data.message;

    esp_err_t err = store->save(send_time, msg_str);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save data to store: %s (0x%x)", esp_err_to_name(err), err);
    }
}

void app_main(void) {
    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    // Initialize databus
    ESP_ERROR_CHECK(databus.init(NODE_ID_SENS_MOCK));

    // Initialize central adapters
    ESP_ERROR_CHECK(store->init());
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    // Hook up databus receive callback to save messages to store
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));

    // Initialize sensor tasks
    static SensorSchedule schedules[] = {
        {"mock", NULL, NULL, pdMS_TO_TICKS(1000)},
    };
    schedules[0].sensor = sensor_mock;
    schedules[0].status_indicator = status_indicator;
    static SensorTaskContext sensor_task_contexts[sizeof(schedules) / sizeof(schedules[0])];
    esp_err_t err = start_sensor_tasks(schedules, sensor_task_contexts, sizeof(schedules) / sizeof(schedules[0]), store,
                                       status_indicator);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sensor tasks: %s (0x%x)", esp_err_to_name(err), err);
        status_indicator->set_status(status_indicator, UNRECOVERABLE_ERROR);
        return;
    }
}
