#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "sim_modem.h"
#include "databus.h"
#include "esp_log.h"
#include "sd_card.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_led.h"

static const char *TAG = "node/cell_gateway";

Intercom *intercom = &sim_modem;
Store *store = &sd_card;
StatusIndicator *status_indicator = &esp_led;

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
    ESP_ERROR_CHECK(databus.init(NODE_ID_CELL_GATEWAY));

    // Initialize central adapters
    ESP_ERROR_CHECK(intercom->init());
    ESP_ERROR_CHECK(store->init());
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    // Hook up databus receive callback to save messages to store
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));

    status_indicator->set_status(status_indicator, OK);

    // Set up intercom task to send data from sd card via intercom
    // TODO
    // BaseType_t task_created = xTaskCreate(send_task, "sim_modem_send", 4096, NULL, 5, NULL);
    // if (task_created != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create modem send task");
    // }

    // Initialize sensor tasks
    // -
}
