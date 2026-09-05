#include "sms.h"

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "sms_gateway";

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    char *seq = "hello world";
    Intercom *intercom_sms = &sms;

    ESP_ERROR_CHECK(intercom_sms->init());

    while (true) { // TODO: add a way to exit this loop
        ESP_ERROR_CHECK(intercom_sms->send(seq, strlen(seq)));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(intercom_sms->deinit());
}