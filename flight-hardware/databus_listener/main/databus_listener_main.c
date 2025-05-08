#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"

#include "databus.h"
#include <stdio.h>

static const char *TAG = "espnow_listener";

void espnow_recv_callback(struct databus_message *msg) { ESP_LOGI(TAG, "Callback %s", msg->log.message); }
void espnow_recv_callback2(struct databus_message *msg) { ESP_LOGI(TAG, "Callback 2 %s", msg->log.message); }

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    databus_wifi_init();
    databus_init();

    databus_register_recv_callback(databus_message_type_log, espnow_recv_callback);
    databus_register_recv_callback(databus_message_type_log, espnow_recv_callback2);

    while (true) {
        ESP_LOGI(TAG, "waiting");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        time_t now;
        time(&now);
        ESP_LOGI(TAG, "time %lld", (unsigned long long)now);

        char buf[128];
        snprintf(buf, sizeof(buf), "here is data !!!! %lld", (unsigned long long)now);

        time_t now_time;
        time(&now_time);
        esp_err_t err = databus_send_data(now_time, buf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "databus_send_data failed with %d %s", err, esp_err_to_name(err));
            return;
        }
    }
}
