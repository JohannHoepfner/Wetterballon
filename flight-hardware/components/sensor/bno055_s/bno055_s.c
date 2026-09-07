#include "bno055_s.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_random.h>
#include <stdio.h>

static const char *TAG = "sensor/bno055";

static char bno055_formatted[64];

Sensor bno055_s = {
    .init = bno055_init,
    .read = bno055_read,
};

esp_err_t bno055_init(void) {
    ESP_LOGI(TAG, "BNO055 sensor initialized");

    return ESP_OK;
}

char *bno055_read(void) {
    double value = ((double)esp_random() / UINT32_MAX) * 100.0;

    ESP_LOGI(TAG, "BNO055 sensor read value: %.2f", value);

    return bno055_format(value);
}

char *bno055_format(double value) {
    snprintf(bno055_formatted, sizeof(bno055_formatted), "bno055=%.2f", value);
    return bno055_formatted;
}
