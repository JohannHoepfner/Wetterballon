#include "include/radio.h"

#include <freertos/FreeRTOS.h>

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <freertos/event_groups.h>

#include "si5351.h"

static const char *TAG = "radio";

si5351_t si5351_dev;

void amp_enable() {
    //
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 1);
}
void amp_disable() {
    //
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 0);
}

esp_err_t radio_init() {
    gpio_set_direction(CONFIG_RADIO_AMP_PWK, GPIO_MODE_OUTPUT);
    amp_disable();
    esp_err_t err = 0;

    ESP_LOGI(TAG, "initing I2C");

    err |= si5351_i2c_init(&si5351_dev, I2C_NUM_0, CONFIG_RADIO_SDA_PIN, CONFIG_RADIO_SCL_PIN, 400000);
    if (err) {
        ESP_LOGI(TAG, "ERROR INIT I2C!");
        return -1;
    }

    ESP_LOGI(TAG, "initing SI5351");

    err |= si5351_init(&si5351_dev, SI5351_CRYSTAL_LOAD_0PF, 25E6, 0);
    if (err) {
        ESP_LOGI(TAG, "ERROR INIT!");
        return -1;
    }

    si5351_output_enable(&si5351_dev, SI5351_CLK0, true);

    return ESP_OK;
}

esp_err_t radio_deinit() { return ESP_OK; }

esp_err_t radio_send_bits(bool *buf, size_t buflen) {
    ESP_LOGI(TAG, "enable amp!");
    amp_enable();
    for (size_t i = 0; i < buflen; ++i) {
        bool bit = buf[i];
        double freq = CONFIG_RADIO_FREQ_BASE + bit * CONFIG_RADIO_FREQ_SHIFT;

        si5351_set_freq(&si5351_dev, freq * 100., SI5351_CLK0);
        ESP_LOGI(TAG, "PRINT %d", bit);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "disable amp!");
    amp_disable();
    return ESP_OK;
}

esp_err_t radio_send_data(char *buf, size_t buflen) {
    bool bits_buf[8];

    for (size_t i = 0; i < buflen; ++i) {
        for (int j = 0; j < 8; ++j) {
            bits_buf[j] = ((buf[i] >> j) & 1);
        }

        radio_send_bits(bits_buf, sizeof(bits_buf));
        ESP_LOGI(TAG, "PRINT CHAR %c", buf[i]);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    return ESP_OK;
}
