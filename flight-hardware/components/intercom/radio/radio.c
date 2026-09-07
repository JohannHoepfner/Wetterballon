#include "radio.h"
#include "si5351.h"

#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/event_groups.h>
#include <stdlib.h>
#include <unistd.h>
#include "radio.h"
#include "radio_frame.h"
#include "rtty.h"
#include "si5351.h"

static const char *TAG = "intercom/radio";

Intercom radio = {
    .init = radio_init,
    .deinit = radio_deinit,
    .send = radio_send_msg,
};

si5351_t si5351_dev;

void amp_enable() {
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 0);
}
void amp_disable() {
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 1);
}

esp_err_t radio_init(void) {
    gpio_set_direction(CONFIG_RADIO_AMP_PWK, GPIO_MODE_OUTPUT);
    amp_disable();

    esp_err_t err = 0;

    ESP_LOGI(TAG, "initing I2C");

    err |= si5351_i2c_init(&si5351_dev, I2C_NUM_0, CONFIG_RADIO_SDA_PIN, CONFIG_RADIO_SCL_PIN, 400000);
    if (err) {
        ESP_LOGI(TAG, "ERROR INIT I2C!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "initing SI5351");

    err |= si5351_init(&si5351_dev, SI5351_CRYSTAL_LOAD_0PF, 25E6, 0);
    if (err) {
        ESP_LOGI(TAG, "ERROR INIT!");
        return ESP_FAIL;
    }

    si5351_drive_strength(&si5351_dev, SI5351_CLK0, SI5351_DRIVE_2MA);

    return ESP_OK;
}

esp_err_t radio_deinit(void) { return ESP_OK; }

static long last_freq = -1;

static int64_t next_bit_us;

static void wait_until(int64_t deadline) {
    while (deadline - esp_timer_get_time() > 2 * portTICK_PERIOD_MS * 1000) {
        vTaskDelay(1);
    }
    while (esp_timer_get_time() < deadline) { /* spin */ }
}

static esp_err_t _radio_send_bit(bool bit) {
    long freq = CONFIG_RADIO_FREQ_BASE + (bit ? CONFIG_RADIO_FREQ_SHIFT : 0);
    wait_until(next_bit_us);
    if (last_freq != freq) {
        si5351_set_freq(&si5351_dev, (uint64_t)freq * SI5351_FREQ_MULT, SI5351_CLK0);
        last_freq = freq;
    }
    next_bit_us += CONFIG_RADIO_BIT_US;
    return ESP_OK;
}

/*
 * One ITA2 character: start bit (space), 5 data bits LSB first, stop bits (mark).
 * The symbol values come straight from rtty_encode_letter() / RTTY_LTRS /
 * RTTY_FIGS, i.e. bit i of the symbol is the i-th bit on the air. That is the
 * same order the old hard-coded rtty_symbol_table used:
 *   'A' -> {1,1,0,0,0} == 0x03 == ALPHABET_LUT['A' - 'A'].
 */
static esp_err_t _radio_send_symbol(uint8_t symbol) {
    _radio_send_bit(false);
    for (uint8_t i = 0; i < 5; ++i) {
        _radio_send_bit((symbol >> i) & 1u);
    }
    for (int i = 0; i < CONFIG_RADIO_STOP_BITS; ++i) {
        _radio_send_bit(true);
    }
    return ESP_OK;
}

/* Keys out an already encoded frame (ITA2 symbols, one per char). */
esp_err_t radio_send_frame(const radio_frame *frame) {
    if (frame == NULL || frame->content == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    amp_enable();
    si5351_output_enable(&si5351_dev, SI5351_CLK0, true);
    usleep(200 * 1000);

    /* Idle mark so the receiver's AGC/decoder can settle before the first start bit */
    _radio_send_bit(true);
    usleep(180 * 1000);

    for (size_t i = 0; i < frame->len; ++i) {
        _radio_send_symbol((uint8_t)frame->content[i]);
    }

    si5351_output_enable(&si5351_dev, SI5351_CLK0, false);
    usleep(200 * 1000);
    amp_disable();
    return ESP_OK;
}

esp_err_t radio_send_msg(char *buf, size_t buflen) {
    radio_frame frame = radio_encode_frame(buf, buflen, true);
    if (frame.content == NULL) {
        ESP_LOGE(TAG, "could not allocate frame for %u byte message", (unsigned)buflen);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "sending %u symbols", (unsigned)frame.len);
    esp_err_t err = radio_send_frame(&frame);

    free(frame.content);
    return err;
}
