#include "radio.h"

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "radio_gateway";

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    char *seq = "hello world";
    Intercom *intercom_radio = &radio;

    ESP_ERROR_CHECK(intercom_radio->init());

    while (true) { // TODO: add a way to exit this loop
        ESP_ERROR_CHECK(intercom_radio->send(seq, strlen(seq)));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(intercom_radio->deinit());
}