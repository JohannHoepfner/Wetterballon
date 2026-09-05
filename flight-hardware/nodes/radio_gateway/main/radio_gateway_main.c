#include "radio.h"
#include "databus.h"
#include "log_store.h"

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "radio_gateway";

void app_main(void) {
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Intracom *intracom = &databus; // TODO: Use for receiving data from other nodes
    Intercom *intercom_radio = &radio;

    char *seq = "hello world";

    ESP_ERROR_CHECK(intercom_radio->init());

    while (true) {
        esp_err_t err = intercom_radio->send(seq, strlen(seq));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send data via intercom: %s (0x%x)",
                     esp_err_to_name(err), err);
        } else {
            ESP_LOGI(TAG, "Sent data via intercom: '%s'", seq);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(intercom_radio->deinit());
}
