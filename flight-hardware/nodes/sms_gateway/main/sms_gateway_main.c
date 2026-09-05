#include "sms.h"

#include <freertos/FreeRTOS.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "sms_gateway";

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    char *seq = "hello world";
    Intercom *intercom_sms = &sms;

    ESP_ERROR_CHECK(intercom_sms->init());

    while (true) { // TODO: add a way to exit this loop
        esp_err_t err = intercom_sms->send(seq, strlen(seq));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send message: %s (0x%x)",
                     esp_err_to_name(err), err);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(60000));
    }

    ESP_ERROR_CHECK(intercom_sms->deinit());
}