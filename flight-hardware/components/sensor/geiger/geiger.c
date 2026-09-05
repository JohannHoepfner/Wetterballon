#include "geiger.h"

#include <esp_err.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <stdio.h>

#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "sensor/geiger";

static volatile uint64_t pulse_count = 0;
static int64_t last_time;
static double frequency;
static char formatted_frequency[32];

static void IRAM_ATTR gpio_isr_handler(void *arg) { pulse_count++; }

Sensor geiger = {
    .init = geiger_init,
    .read = geiger_read,
};

esp_err_t geiger_init(Sensor *self) {
    (void)self;

    esp_err_t err;

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << CONFIG_GEIGER_PULSE_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    err = gpio_config(&io_conf);
    if (err) {
        return err;
    }

    err = gpio_install_isr_service(0);
    if (err) {
        return err;
    }

    last_time = esp_timer_get_time();

    gpio_isr_handler_add(CONFIG_GEIGER_PULSE_PIN, gpio_isr_handler, NULL);
    if (err) {
        return err;
    }

    ESP_LOGI(TAG, "Geiger sensor initialized on pin %d", CONFIG_GEIGER_PULSE_PIN);

    return ESP_OK;
}

char *geiger_read(Sensor *self) {
    (void)self;

    int64_t time = esp_timer_get_time();
    double dt = time - last_time;
    last_time = time;
    frequency = pulse_count / dt;
    pulse_count = 0;

    ESP_LOGI(TAG, "Geiger sensor read: frequency=%.2f Hz", frequency);

    return geiger_format(frequency);
}

char *geiger_format(double value) {
    snprintf(formatted_frequency, sizeof(formatted_frequency), "s=%.2f", value);
    return formatted_frequency;
}
