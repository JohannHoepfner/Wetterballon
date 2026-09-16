#include "databus.h"
#include "esp_led.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "node/debug_station";

struct status_indicator *status_indicator = &esp_led;

static void
on_databus_data(struct databus_message *message) {
    if (message == NULL || message->type != DATABUS_MSG_TYPE_DATA) {
        ESP_LOGE(TAG, "Invalid databus data message");
        return;
    }

    time_t send_time = message->send_time;
    char *msg_str = message->DATA_content.message;

    ESP_LOGI(TAG, "databus data message: %ld:'%s' from '%s' (msg_id: %llu)", (long)send_time, msg_str,
             databus_get_node_name(message->node_id), (unsigned long long)message->msg_id);
}

static void
on_databus_log(struct databus_message *message) {
    if (message == NULL || message->type != DATABUS_MSG_TYPE_LOG) {
        ESP_LOGE(TAG, "Invalid databus log message");
        return;
    }

    time_t send_time = message->send_time;
    char *msg_str = message->LOG_content.message;

    ESP_LOGI(TAG, "databus log message: %ld:'%s' from '%s' (msg_id: %llu)", (long)send_time, msg_str,
             databus_get_node_name(message->node_id), (unsigned long long)message->msg_id);
}

static void
on_databus_timesync(struct databus_message *message) {
    if (message == NULL || message->type != DATABUS_MSG_TYPE_TIMESYNC) {
        ESP_LOGE(TAG, "Invalid databus timesync message");
        return;
    }

    time_t new_time = message->TIMESYNC_content.time;

    ESP_LOGI(TAG, "Received timesync message with time: %ld from '%s' (msg_id: %llu)", (long)new_time,
             databus_get_node_name(message->node_id), (unsigned long long)message->msg_id);
}

static void
on_databus_any(struct databus_message *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid databus message");
        return;
    }

    time_t send_time = message->send_time;

    ESP_LOGW(TAG, "databus message of type %d at %ld from '%s', (msg_id: %llu)", message->type, (long)send_time,
             databus_get_node_name(message->node_id), (unsigned long long)message->msg_id);
}

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
    ESP_ERROR_CHECK(databus.init(NODE_ID_SENS_MOCK));

    // Initialize central adapters
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    // Hook up databus receive callbacks
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_LOG, on_databus_log));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_TIMESYNC, on_databus_timesync));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_KILL_RADIO, on_databus_any));

    status_indicator->set_status(status_indicator, STATUS_INDICATOR_OK);
}
