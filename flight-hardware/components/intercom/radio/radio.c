#include "radio.h"
#include "si5351.h"

#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <freertos/event_groups.h>
#include <unistd.h>

static const char *TAG = "intercom/radio";

Intercom radio = {
    .init = radio_init,
    .deinit = radio_deinit,
    .send = radio_send_msg
};

si5351_t si5351_dev;

void amp_enable() {
    //
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 1);
    usleep(200 * 1000);
}
void amp_disable() {
    //
    usleep(200 * 1000);
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 0);
}

esp_err_t radio_init(void) {
    // gpio_set_direction(CONFIG_RADIO_AMP_PWK, GPIO_MODE_OUTPUT);
    // amp_disable();

    // esp_err_t err = 0;

    // ESP_LOGI(TAG, "initing I2C");

    // err |= si5351_i2c_init(&si5351_dev, I2C_NUM_0, CONFIG_RADIO_SDA_PIN, CONFIG_RADIO_SCL_PIN, 400000);
    // if (err) {
    //     ESP_LOGI(TAG, "ERROR INIT I2C!");
    //     return -1;
    // }

    // ESP_LOGI(TAG, "initing SI5351");

    // err |= si5351_init(&si5351_dev, SI5351_CRYSTAL_LOAD_0PF, 25E6, 0);
    // if (err) {
    //     ESP_LOGI(TAG, "ERROR INIT!");
    //     return -1;
    // }
    ESP_LOGI(TAG, "Radio intercom adapter initialized");

    return ESP_OK;
}

esp_err_t radio_deinit(void) {
    ESP_LOGI(TAG, "Radio intercom adapter deinitialized");

    return ESP_OK;
}

long last_freq = -1;
esp_err_t _radio_send_bit(bool bit) {
    long freq = CONFIG_RADIO_FREQ_BASE + (bit ? CONFIG_RADIO_FREQ_SHIFT : 0);
    if (last_freq != freq) {
        si5351_set_freq(&si5351_dev, freq * 100., SI5351_CLK0);
        last_freq = freq;
    }
    usleep(20 * 1000);
    return ESP_OK;
}

esp_err_t _radio_send_bits(bool buf[5]) {
    _radio_send_bit(0);
    for (size_t i = 0; i < 5; ++i) {
        _radio_send_bit(buf[i]);
    }
    _radio_send_bit(1);
    _radio_send_bit(1);
    return ESP_OK;
}

bool rtty_symbol_table[][5] = {
    {1, 1, 0, 0, 0}, // 0  A
    {1, 0, 0, 1, 1}, // 1  B
    {0, 1, 1, 1, 0}, // 2  C
    {1, 0, 0, 1, 0}, // 3  D
    {1, 0, 0, 0, 0}, // 4  E
    {1, 0, 1, 1, 0}, // 5  F
    {0, 1, 0, 1, 1}, // 6  G
    {0, 0, 1, 0, 1}, // 7  H
    {0, 1, 1, 0, 0}, // 8  I
    {1, 1, 0, 1, 0}, // 9  J
    {1, 1, 1, 1, 0}, // 10 K
    {0, 1, 0, 0, 1}, // 11 L
    {0, 0, 1, 1, 1}, // 12 M
    {0, 0, 1, 1, 0}, // 13 N
    {0, 0, 0, 1, 1}, // 14 O
    {0, 1, 1, 0, 1}, // 15 P
    {1, 1, 1, 0, 1}, // 16 Q
    {0, 1, 0, 1, 0}, // 17 R
    {1, 0, 1, 0, 0}, // 18 S
    {0, 0, 0, 0, 1}, // 19 T
    {1, 1, 1, 0, 0}, // 20 U
    {0, 1, 1, 1, 1}, // 21 V
    {1, 1, 0, 0, 1}, // 22 W
    {1, 0, 1, 1, 1}, // 23 X
    {1, 0, 1, 0, 1}, // 24 Y
    {1, 0, 0, 0, 1}, // 25 Z
    {0, 1, 1, 0, 1}, // 26 0
    {1, 1, 1, 0, 1}, // 27 1
    {1, 1, 0, 0, 1}, // 28 2
    {1, 0, 0, 0, 0}, // 29 3
    {0, 1, 0, 1, 0}, // 30 4
    {0, 0, 0, 0, 1}, // 31 5
    {1, 0, 1, 0, 1}, // 32 6
    {1, 1, 1, 0, 0}, // 33 7
    {0, 1, 1, 0, 0}, // 34 8
    {0, 0, 0, 1, 1}, // 35 9
    {0, 0, 1, 0, 0}, // 36 SPACE
    {0, 0, 0, 1, 0}, // 37 CR
    {0, 1, 0, 0, 0}, // 38 LF
    {1, 1, 0, 1, 1}, // 39 NUMBERS
    {1, 1, 1, 1, 1}  // 40 LETTERS
};

esp_err_t radio_send_msg(char *buf, size_t buflen) {
    // Don't send if the buffer is empty or null
    if (buf == NULL || buflen == 0) {
        ESP_LOGW(TAG, "radio_send_msg: buffer is NULL or empty");
        return ESP_OK;
    }

    // amp_enable();
    // si5351_output_enable(&si5351_dev, SI5351_CLK0, true);

    // _radio_send_bit(1);
    // usleep(180 * 1000);

    // _radio_send_bits(rtty_symbol_table[37]); // CR
    // _radio_send_bits(rtty_symbol_table[38]); // LF

    // bool numbersMode = false;
    // bool numbersModePrev = false;

    // for (size_t i_char = 0; i_char < buflen; ++i_char) {
    //     char c = buf[i_char];

    //     int symbolIndex = 0;
    //     int symbolShift = 65;
    //     if (c < 58 && c != 32) {
    //         numbersMode = true;
    //         symbolShift = 22;
    //         if (!numbersModePrev) {
    //             _radio_send_bits(rtty_symbol_table[39]);
    //         }
    //     } else {
    //         // Letters
    //         numbersMode = false;
    //         if (numbersModePrev) {
    //             _radio_send_bits(rtty_symbol_table[40]);
    //         }
    //     }
    //     numbersModePrev = numbersMode;
    //     symbolIndex = c - symbolShift;

    //     if (c == 32) {
    //         symbolIndex = 36;
    //     }
    //     _radio_send_bits(rtty_symbol_table[symbolIndex]);
    // }

    // _radio_send_bits(rtty_symbol_table[37]); // CR
    // _radio_send_bits(rtty_symbol_table[38]); // LF

    // si5351_output_enable(&si5351_dev, SI5351_CLK0, false);
    // amp_disable();
    ESP_LOGI(TAG, "radio mock send: '%.*s'", (int)buflen, buf);

    return ESP_OK;
}
