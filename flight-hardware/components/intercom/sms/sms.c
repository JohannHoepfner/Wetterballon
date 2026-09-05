#include "sms.h"

#include <esp_log.h>

static const char *TAG = "sms";

Intercom sms = {
    .init = sms_init,
    .deinit = sms_deinit,
    .send = sms_send_msg
};

esp_err_t sms_init(void) {
    return ESP_OK;
}

esp_err_t sms_deinit(void) {
    return ESP_OK;
}

esp_err_t sms_send_msg(char *buf, size_t buflen) {
    ESP_LOGI(TAG, "Sending SMS: %.*s", buflen, buf);
    return ESP_OK;
}
