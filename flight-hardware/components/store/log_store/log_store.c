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
    .load = no_load,
};

esp_err_t log_init(void) {
    ESP_LOGI(TAG, "Log adapter initialized");

    return ESP_OK;
}

esp_err_t log_deinit() {
    ESP_LOGI(TAG, "Log adapter deinitialized");

    return ESP_OK;
}

esp_err_t log_data(time_t time, char *msg_str) {
    ESP_LOGI(TAG, "Log message: send_time=%llu, type=%u, message='%s'", (unsigned long long)time, DATABUS_MSG_TYPE_DAT, msg_str);

    return ESP_OK;
}

ssize_t no_load(struct databus_message *out_messages, size_t start, size_t count) {
    ESP_LOGI(TAG, "Log adapter does not support loading messages");

    return -1;
}
