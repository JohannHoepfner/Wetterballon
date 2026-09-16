#include "databus.h"
#include "esp_led.h"
#include "mock.h"
#include "mock_store.h"
#include "pt1000.h"
#include "sd_card.h"
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

static const char *TAG = "node/sens_misc";

static struct store_sd_card store_sd_card;
static struct store *store;

struct status_indicator *const status_indicator = &esp_led;

static void
on_databus_data(struct databus_message *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid databus message");
        return;
    }

    if (store == NULL) {
        ESP_LOGE(TAG, "Store is not initialized");
        return;
    }

    time_t send_time = message->send_time;
    char *msg_str = message->DATA_content.message;

    esp_err_t err = store->save(store, send_time, msg_str);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save data to store: %s (0x%x)", esp_err_to_name(err), err);
    }
}

static struct sensor_schedule schedules[] = {
    {"pt1000", &pt1000, status_indicator, pdMS_TO_TICKS(1000)},
};

void
app_main(void) {
    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    // Initialize databus
    ESP_ERROR_CHECK(databus.init(NODE_ID_SENS_MISC));

    // Initialize central adapters
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    sd_card_store_create(&store_sd_card, &(struct store_sd_card_cfg){
                                             .pin_mosi = GPIO_NUM_5,
                                             .pin_miso = GPIO_NUM_4,
                                             .pin_clk = GPIO_NUM_10,
                                             .pin_cs = GPIO_NUM_11,
                                         });
    store = &store_sd_card.base;
    esp_err_t store_init_err = store->init(store);
    if (store_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize store: %s (0x%x)", esp_err_to_name(store_init_err), store_init_err);
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SD_CARD_ERROR);
        ESP_LOGW(TAG, "Continuing without store functionality (using mock store)");
        store = &mock_store;
        ESP_ERROR_CHECK(store->init(store));
    }

    // Hook up databus receive callback to save messages to store
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_TIMESYNC, on_databus_timesync_default));

    static SensorTaskContext sensor_task_contexts[sizeof(schedules) / sizeof(schedules[0])];
    esp_err_t err = start_sensor_tasks(schedules, sensor_task_contexts, sizeof(schedules) / sizeof(schedules[0]), store,
                                       status_indicator);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sensor tasks: %s (0x%x)", esp_err_to_name(err), err);
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
        return;
    }
}
