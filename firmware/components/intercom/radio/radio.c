#include "radio.h"
#include "si5351.h"

#include "radio.h"
#include "radio_frame.h"
#include "rtty.h"
#include "si5351.h"
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TAG = "intercom/radio";

Intercom radio = {
    .init = radio_init,
    .deinit = radio_deinit,
    .send = radio_send_msg,
};

si5351_t si5351_dev;

void
amp_enable() {
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 1);
}
void
amp_disable() {
    gpio_set_level(CONFIG_RADIO_AMP_PWK, 0);
}

#define FSK_XTAL_HZ 25000000LL /* must match the value passed to si5351_init() */
#define FSK_CORR_PPB 0         /* your measured calibration, parts per billion */
#define FSK_FRAC_C 1048575UL   /* max fractional denominator (2^20 - 1)        */
#define SI5351_PLLA_REGS 26

typedef struct {
    uint8_t r[8];
} pll_regs_t; /* registers 26..33 */

static pll_regs_t reg_mark, reg_space;
static i2c_master_dev_handle_t fsk_dev;
static uint8_t oeb_shadow; /* cached register 3 */

static void
pll_regs_for(uint64_t fvco, pll_regs_t *out) {
    const int64_t fx = FSK_XTAL_HZ + (FSK_XTAL_HZ * FSK_CORR_PPB) / 1000000000LL;

    uint32_t a = (uint32_t)(fvco / (uint64_t)fx);
    uint64_t rem = fvco % (uint64_t)fx;
    uint32_t c = FSK_FRAC_C;
    uint32_t b = (uint32_t)((rem * c + fx / 2) / fx); /* rounded, not truncated */

    uint32_t f = (uint32_t)((128ULL * b) / c);
    uint32_t p1 = 128 * a + f - 512;
    uint32_t p2 = (uint32_t)(128ULL * b - (uint64_t)c * f);
    uint32_t p3 = c;

    out->r[0] = (p3 >> 8) & 0xFF;
    out->r[1] = p3 & 0xFF;
    out->r[2] = (p1 >> 16) & 0x03;
    out->r[3] = (p1 >> 8) & 0xFF;
    out->r[4] = p1 & 0xFF;
    out->r[5] = ((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F);
    out->r[6] = (p2 >> 8) & 0xFF;
    out->r[7] = p2 & 0xFF;
}

static esp_err_t
fsk_setup(void) {
    const uint64_t f_space = CONFIG_RADIO_FREQ_BASE;
    const uint64_t f_mark = CONFIG_RADIO_FREQ_BASE + CONFIG_RADIO_FREQ_SHIFT;

    /* Largest even output divider keeping the VCO inside 600..900 MHz. */
    uint32_t d = (uint32_t)(900000000ULL / f_mark);
    if (d & 1u)
        d--;
    if (d < 6 || d > 900 || f_space * d < 600000000ULL) {
        ESP_LOGE(TAG, "no usable multisynth divider for %llu Hz", f_space);
        return ESP_ERR_INVALID_ARG; /* >150 MHz needs DIVBY4, <1 MHz needs an R divider */
    }

    pll_regs_for(f_space * d, &reg_space);
    pll_regs_for(f_mark * d, &reg_mark);

    /* Program the multisynth exactly once, PLL parked on the space tone. */
    si5351_set_freq_manual(&si5351_dev, f_space * SI5351_FREQ_MULT, f_space * d * SI5351_FREQ_MULT, SI5351_CLK0);
    si5351_set_int(&si5351_dev, SI5351_CLK0, 1); /* MS is an even integer -> less jitter */
    si5351_pll_reset(&si5351_dev, SI5351_PLLA);
    si5351_output_enable(&si5351_dev, SI5351_CLK0, false);

    ESP_LOGI(TAG, "MS divider %lu, tuning step %.2f Hz", (unsigned long)d,
             (double)FSK_XTAL_HZ / ((double)FSK_FRAC_C * d));
    return ESP_OK;
}

static esp_err_t
fsk_open(void) {
    esp_err_t e =
        i2c_master_bus_add_device(si5351_dev.i2c_dev.i2c_bus_handle, &si5351_dev.i2c_dev.i2c_dev_conf, &fsk_dev);
    if (e != ESP_OK)
        return e;
    uint8_t addr = 3;
    return i2c_master_transmit_receive(fsk_dev, &addr, 1, &oeb_shadow, 1, SI5351_I2C_TIMEOUT_MS);
}

static void
fsk_close(void) {
    i2c_master_bus_rm_device(fsk_dev);
    fsk_dev = NULL;
}

static inline esp_err_t
fsk_tone(bool mark) {
    uint8_t buf[9];
    buf[0] = SI5351_PLLA_REGS;
    memcpy(&buf[1], mark ? reg_mark.r : reg_space.r, 8);
    return i2c_master_transmit(fsk_dev, buf, sizeof(buf), SI5351_I2C_TIMEOUT_MS);
}

static esp_err_t
fsk_output(bool on) {
    uint8_t v = on ? (uint8_t)(oeb_shadow & ~1u) : (uint8_t)(oeb_shadow | 1u);
    uint8_t buf[2] = {3, v};
    esp_err_t e = i2c_master_transmit(fsk_dev, buf, 2, SI5351_I2C_TIMEOUT_MS);
    if (e == ESP_OK)
        oeb_shadow = v;
    return e;
}

esp_err_t
radio_init(void) {
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

    si5351_drive_strength(&si5351_dev, SI5351_CLK0, SI5351_DRIVE_4MA);

    fsk_setup();

    return ESP_OK;
}

esp_err_t
radio_deinit(void) {
    return ESP_OK;
}

static int64_t next_bit_us;

static void
wait_until(int64_t deadline) {
    while (deadline - esp_timer_get_time() > 2 * portTICK_PERIOD_MS * 1000) {
        vTaskDelay(1);
    }
    while (esp_timer_get_time() < deadline) {
    }
}

static int last_tone = -1;

static esp_err_t
_radio_send_bit(bool bit) {
    wait_until(next_bit_us);
    if (last_tone != (int)bit) {
        fsk_tone(bit);
        last_tone = bit;
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
static esp_err_t
_radio_send_symbol(uint8_t symbol) {
    _radio_send_bit(false);
    for (uint8_t i = 0; i < 5; ++i) {
        _radio_send_bit((symbol >> i) & 1u);
    }
    for (int i = 0; i < CONFIG_RADIO_STOP_BITS; ++i) {
        _radio_send_bit(true);
    }
    return ESP_OK;
}

esp_err_t
radio_send_frame(const radio_frame *frame) {
    if (frame == NULL || frame->content == NULL)
        return ESP_ERR_INVALID_ARG;

    amp_enable();
    usleep(100 * 1000);

    if (fsk_open() != ESP_OK) {
        amp_disable();
        return ESP_FAIL;
    }

    fsk_tone(true); /* park on mark before the carrier comes up */
    last_tone = 1;
    fsk_output(true);

    next_bit_us = esp_timer_get_time() + 1000 * 1000; /* 1 s steady mark */

    for (size_t i = 0; i < frame->len; ++i)
        _radio_send_symbol((uint8_t)frame->content[i]);

    wait_until(next_bit_us + 200 * 1000);
    fsk_output(false);
    fsk_close();

    usleep(100 * 1000);
    amp_disable();
    return ESP_OK;
}

esp_err_t
radio_send_msg(char *buf, size_t buflen) {
    ESP_LOGI(TAG, "sending %u chars '%s'", (unsigned)buflen, buf);
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
