#include "bme280_s.h"

#include <bme280.h>
#include <i2c_bus.h>

#include <driver/gpio.h>
#include <esp_log.h>
#include <sdkconfig.h>
#include <stdio.h>

static const char *TAG = "sensor/bme280";

static const i2c_port_t I2C_PORT = I2C_NUM_0;
static const gpio_num_t SDA_PIN = CONFIG_BME280_SDA_PIN;
static const gpio_num_t SCL_PIN = CONFIG_BME280_SCL_PIN;
static const uint8_t PRIMARY_ADDRESS = 0x76;
static const uint8_t SECONDARY_ADDRESS = 0x77;

static i2c_bus_handle_t i2c_bus;
static bme280_handle_t sensor;
static char formatted_reading[64];

struct sensor bme280_s = {
    .init = bme280_init,
    .read = bme280_read,
};

esp_err_t
bme280_init(void) {
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_bus = i2c_bus_create(I2C_PORT, &config);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to initialize I2C on SDA %d, SCL %d", CONFIG_BME280_SDA_PIN, CONFIG_BME280_SCL_PIN);
        return ESP_FAIL;
    }

    sensor = bme280_create(i2c_bus, PRIMARY_ADDRESS);
    if (sensor != NULL && bme280_default_init(sensor) == ESP_OK) {
        return ESP_OK;
    }
    bme280_delete(&sensor);

    sensor = bme280_create(i2c_bus, SECONDARY_ADDRESS);
    if (sensor == NULL || bme280_default_init(sensor) != ESP_OK) {
        bme280_delete(&sensor);
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

char *
bme280_read(void) {
    float temperature = 0.0F;
    float pressure = 0.0F;
    float humidity = 0.0F;

    if (bme280_read_temperature(sensor, &temperature) != ESP_OK || bme280_read_pressure(sensor, &pressure) != ESP_OK ||
        bme280_read_humidity(sensor, &humidity) != ESP_OK) {
        return NULL;
    }

    snprintf(formatted_reading, sizeof(formatted_reading), "t2=%.2f,p=%.2f,h=%.2f", temperature, pressure, humidity);
    ESP_LOGI(TAG, "BME280 reading: %s", formatted_reading);

    return formatted_reading;
}
