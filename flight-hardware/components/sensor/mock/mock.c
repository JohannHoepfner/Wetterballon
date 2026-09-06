#include "mock.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_random.h>
#include <stdio.h>

static const char *TAG = "sensor/mock";

typedef struct MockSensor {
    double value;
} MockSensor;

static MockSensor mock_context;
static char mock_formatted[32];

Sensor mock = {
    .ctx = &mock_context,
    .init = mock_init,
    .read = mock_read,
};

esp_err_t mock_init(Sensor *self) {
    MockSensor *mock = (MockSensor *)self->ctx;

    mock->value = (double)(esp_random() % 1000) / 100.0;

    ESP_LOGI(TAG, "Mock sensor initialized with value: %.2f", mock->value);

    return ESP_OK;
}

char *mock_read(Sensor *self) {
    MockSensor *mock = (MockSensor *)self->ctx;

    double value_red = mock->value;

    ESP_LOGI(TAG, "Mock sensor read value: %.2f", value_red);

    return mock_format(value_red);
}

char *mock_format(double value) {
    snprintf(mock_formatted, sizeof(mock_formatted), "m=%.2f", value);
    return mock_formatted;
}
