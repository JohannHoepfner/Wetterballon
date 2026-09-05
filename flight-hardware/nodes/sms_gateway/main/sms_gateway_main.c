#include "sms.h"
#include "sd_card.h"

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
    
    // char *seq = "hello world";

    // Mock databus message for testing
    struct databus_message msg = {
        .send_time = 0,
        .type = DATABUS_MSG_TYPE_DAT,
        .data = {.message = "hello world"}
    };
    
    Intercom *intercom_sms = &sms;
    Store *store_sd_card = &sd_card;

    ESP_ERROR_CHECK(intercom_sms->init());
    ESP_ERROR_CHECK(store_sd_card->init());

    while (true) { // TODO: add a way to exit this loop
        // esp_err_t err = intercom_sms->send(seq, strlen(seq));
        // if (err != ESP_OK) {
        //     ESP_LOGE(TAG, "Failed to send message: %s (0x%x)",
        //              esp_err_to_name(err), err);
        //     continue;
        // }

        esp_err_t err2 = store_sd_card->save(&msg);
        if (err2 != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save message to SD card: %s (0x%x)",
                     esp_err_to_name(err2), err2);
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(600));
    }

    ESP_ERROR_CHECK(intercom_sms->deinit());
    ESP_ERROR_CHECK(store_sd_card->deinit());
}