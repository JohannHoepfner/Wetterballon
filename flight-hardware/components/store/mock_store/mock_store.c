#include "mock_store.h"

#include "databus.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "store/mock_store";

Store mock_store = {
    .init = log_init,
    .deinit = log_deinit,
    .save = log_data,
};

esp_err_t log_init(void) {
    ESP_LOGI(TAG, "Mock store adapter initialized");

    return ESP_OK;
}

esp_err_t log_deinit() {
    ESP_LOGI(TAG, "Mock store adapter deinitialized");

    return ESP_OK;
}

esp_err_t log_data(time_t time, char *msg_str) {
    ESP_LOGI(TAG, "Mock store adapter write data: send_time=%llu, message='%s'", (unsigned long long)time, msg_str);

    return ESP_OK;
}
