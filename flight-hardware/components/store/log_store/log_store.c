#include "log_store.h"

#include "esp_log.h"
#include "esp_err.h"
#include "databus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "store/log_store";

Store log_store = {
    .init = log_init,
    .deinit = log_deinit,
    .save = log_data,
    .load = no_read,
};

esp_err_t log_init(void) {
    ESP_LOGI(TAG, "Log store adapter initialized");

    return ESP_OK;
}

esp_err_t log_deinit() {
    ESP_LOGI(TAG, "Log store adapter deinitialized");

    return ESP_OK;
}

esp_err_t log_data(time_t time, char *msg_str) {
    ESP_LOGI(TAG, "Mock store write data: send_time=%llu, message='%s'", (unsigned long long)time, msg_str);

    return ESP_OK;
}

ssize_t no_read(struct databus_message *out_messages, size_t start, size_t count) {
    ESP_LOGI(TAG, "Log store adapter does not support loading messages");

    return -1;
}
