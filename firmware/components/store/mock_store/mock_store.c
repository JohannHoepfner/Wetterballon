#include "mock_store.h"

#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "store/mock_store";

static esp_err_t mock_store_init(struct store *) {
    ESP_LOGI(TAG, "Mock store adapter initialized");

    return ESP_OK;
}

static esp_err_t mock_store_deinit(void) {
    ESP_LOGI(TAG, "Mock store adapter deinitialized");

    return ESP_OK;
}

static esp_err_t mock_store_save(struct store *, time_t time, const char *msg_str) {
    ESP_LOGI(TAG, "Mock store adapter write data: send_time=%llu, message='%s'", (unsigned long long)time, msg_str);

    return ESP_OK;
}

struct store mock_store = {
    .init = mock_store_init,
    .reinit = mock_store_init,
    .deinit = mock_store_deinit,
    .save = mock_store_save,
    .read_lines = NULL, // Not implemented for mock store
};
