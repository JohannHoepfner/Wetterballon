#include "databus_message.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"

#include "databus.h"

static const char *TAG = "espnow_listener";

void espnow_recv_callback_log(struct databus_message *msg) { ESP_LOGI(TAG, "LOG %s", msg->log.message); }
void espnow_recv_callback_dat(struct databus_message *msg) { ESP_LOGI(TAG, "DAT %s", msg->data.message); }

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    databus_wifi_init();
    databus_init();

    databus_register_recv_callback(DATABUS_MSG_TYPE_LOG, espnow_recv_callback_log);
    databus_register_recv_callback(DATABUS_MSG_TYPE_DAT, espnow_recv_callback_dat);

    while (true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);

        time_t now;
        time(&now);

        // char buf[128];
        // snprintf(buf, sizeof(buf), "here is data !!!! %lld", (unsigned long long)now);

        // time_t now_time;
        // time(&now_time);
        // esp_err_t err = databus_send_message(now_time, buf);
        // if (err != ESP_OK) {
        //     ESP_LOGE(TAG, "databus_send_message failed with %d %s", err, esp_err_to_name(err));
        //     return;
        // }
    }
}
