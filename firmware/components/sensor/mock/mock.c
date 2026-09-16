#include "mock.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_random.h>
#include <stdio.h>

static const char *TAG = "sensor/mock";

static char mock_formatted[32];

struct sensor mock = {
    .init = mock_init,
    .read = mock_read,
};

esp_err_t
mock_init(void) {
    ESP_LOGI(TAG, "Mock sensor initialized");

    return ESP_OK;
}

char *
mock_read(void) {
    double value = ((double)esp_random() / UINT32_MAX) * 100.0;

    ESP_LOGI(TAG, "Mock sensor read value: %.2f", value);

    snprintf(mock_formatted, sizeof(mock_formatted), "m=%.2f", value);
    return mock_formatted;
}

char *
mock_format(double value) {
    static char formatted[32];

    snprintf(formatted, sizeof(formatted), "m=%.2f", value);
    return formatted;
}
