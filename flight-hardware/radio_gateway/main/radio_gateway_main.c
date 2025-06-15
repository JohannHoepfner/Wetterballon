#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "radio.h"

#include <freertos/FreeRTOS.h>

#include <stdbool.h>
#include <string.h>

static const char *TAG = "radio_gateway";

void app_main(void) {

    char *seq = "hello world";

    ESP_ERROR_CHECK(radio_init());

    while (true) {
        radio_send_data(seq, strlen(seq));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
