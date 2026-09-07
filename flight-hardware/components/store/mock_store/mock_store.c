#include "mock_store.h"

#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "store/mock_store";

Store mock_store = {
    .init = mock_store_init,
    .deinit = mock_store_deinit,
    .save = mock_store_save,
    .read_lines = NULL, // Not implemented for mock store
};

esp_err_t mock_store_init(void) {
    ESP_LOGI(TAG, "Mock store adapter initialized");

    return ESP_OK;
}

esp_err_t mock_store_deinit() {
    ESP_LOGI(TAG, "Mock store adapter deinitialized");

    return ESP_OK;
}

esp_err_t mock_store_save(time_t time, char *msg_str) {
    ESP_LOGI(TAG, "Mock store adapter write data: send_time=%llu, message='%s'", (unsigned long long)time, msg_str);

    return ESP_OK;
}
