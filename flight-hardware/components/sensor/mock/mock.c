#include "../sensor.h"

#include "mock.h"

#include <esp_err.h>

typedef struct MockSensor {
    double value;
} MockSensor;

Sensor mock_sensor = {
    .init = mock_init,
    .read = mock_read,
};

static esp_err_t mock_init(Sensor *self)
{
    MockSensor *mock = (MockSensor *)self;

    mock->value = (double)(esp_random() % 1000) / 100.0;

    return ESP_OK;
}

static double mock_read(Sensor *self)
{
    MockSensor *mock = (MockSensor *)self;
    return mock->value;
}

