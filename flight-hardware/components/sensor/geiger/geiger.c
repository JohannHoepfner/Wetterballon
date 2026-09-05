#include "geiger.h"

#include <esp_err.h>
#include <esp_timer.h>
#include <esp_log.h>

#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static volatile uint64_t pulse_count = 0;
static int64_t last_time;

static void IRAM_ATTR gpio_isr_handler(void *arg) { pulse_count++; }

Sensor geiger_sensor = {
    .init = geiger_init,
    .read = geiger_get_freq,
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

    return ESP_OK;
}

double geiger_get_freq(Sensor *self) {
    (void)self;

    int64_t time = esp_timer_get_time();
    double dt = time - last_time;
    last_time = time;
    double freq = pulse_count / dt;
    return freq;
}
