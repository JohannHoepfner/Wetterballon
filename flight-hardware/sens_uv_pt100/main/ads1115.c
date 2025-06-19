#include "ads1115.h"
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "hal/gpio_types.h"

#include "driver/i2c_master.h"

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    const bool ret = 1;                                // dummy value to pass to queue
    QueueHandle_t gpio_evt_queue = (QueueHandle_t)arg; // find which queue to write
    xQueueSendFromISR(gpio_evt_queue, &ret, NULL);
}

static esp_err_t ads1115_write_register(ads1115_t *ads, ads1115_register_addresses_t reg, uint16_t data) {

    uint8_t out[3];
    out[0] = reg;
    out[1] = data >> 8;   // get 8 greater bits
    out[2] = data & 0xFF; // get 8 lower bits

    esp_err_t err = i2c_master_transmit(ads->i2c_dev_handle, out, sizeof(out), ads->xfer_timeout_ms);

    if (err == ESP_OK) {
        ads->last_reg = reg;
    }

    return err;
}

static esp_err_t ads1115_read_register(ads1115_t *ads, ads1115_register_addresses_t reg, uint8_t *data, uint8_t len) {

    if (ads->last_reg != reg) { // if we're not on the correct register, change it
        ESP_ERROR_CHECK(i2c_master_transmit(ads->i2c_dev_handle, (uint8_t *)&reg, 1, ads->xfer_timeout_ms));
        ads->last_reg = reg;
    }

    return i2c_master_receive(ads->i2c_dev_handle, data, len, ads->xfer_timeout_ms);
}

ads1115_t ads1115_config(i2c_master_dev_handle_t i2c_dev_handle) {
    ads1115_t ads;         // setup configuration with default values
    ads.config.bit.OS = 1; // always start conversion
    ads.config.bit.MUX = ADS1115_MUX_0_GND;
    ads.config.bit.PGA = ADS1115_FSR_4_096;
    ads.config.bit.MODE = ADS1115_MODE_SINGLE;
    ads.config.bit.DR = ADS1115_SPS_64;
    ads.config.bit.COMP_MODE = 0;
    ads.config.bit.COMP_POL = 0;
    ads.config.bit.COMP_LAT = 0;
    ads.config.bit.COMP_QUE = 0b11;

    ads.i2c_dev_handle = i2c_dev_handle;
    ads.rdy_pin.in_use = 0;                   // state that rdy_pin not used
    ads.last_reg = ADS1115_MAX_REGISTER_ADDR; // say that we accessed invalid register last
    ads.changed = 1;                          // say we changed the configuration
    ads.xfer_timeout_ms = 10;
    return ads; // return the completed configuration
}

void ads1115_set_mux(ads1115_t *ads, ads1115_mux_t mux) {
    ads->config.bit.MUX = mux;
    ads->changed = 1;
}

void ads1115_set_rdy_pin(ads1115_t *ads, gpio_num_t gpio) {
    const static char *TAG = "ads1115_set_rdy_pin";
    esp_err_t err;

    ESP_ERROR_CHECK(gpio_set_direction(gpio, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_intr_type(gpio, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_set_pull_mode(gpio, GPIO_PULLUP_ENABLE));

    ads->rdy_pin.gpio_evt_queue = xQueueCreate(1, sizeof(bool));
    gpio_install_isr_service(0);

    ads->rdy_pin.in_use = 1;
    ads->rdy_pin.pin = gpio;
    ads->config.bit.COMP_QUE = 0b00; // assert after one conversion
    ads->changed = 1;

    err = ads1115_write_register(ads, ADS1115_LO_THRESH_REGISTER_ADDR, 0); // set lo threshold to minimum
    if (err)
        ESP_LOGE(TAG, "could not set low threshold: %s", esp_err_to_name(err));
    err = ads1115_write_register(ads, ADS1115_HI_THRESH_REGISTER_ADDR, 0xFFFF); // set hi threshold to maximum
    if (err)
        ESP_LOGE(TAG, "could not set high threshold: %s", esp_err_to_name(err));
}

void ads1115_set_pga(ads1115_t *ads, ads1115_fsr_t fsr) {
    ads->config.bit.PGA = fsr;
    ads->changed = 1;
}

void ads1115_set_mode(ads1115_t *ads, ads1115_mode_t mode) {
    ads->config.bit.MODE = mode;
    ads->changed = 1;
}

void ads1115_set_sps(ads1115_t *ads, ads1115_sps_t sps) {
    ads->config.bit.DR = sps;
    ads->changed = 1;
}

void ads1115_set_timeout_ms(ads1115_t *ads, int timeout_ms) { ads->xfer_timeout_ms = timeout_ms; }

int16_t ads1115_get_raw(ads1115_t *ads) {
    const static char *TAG = "ads1115_get_raw";
    const static uint16_t sps[] = {8, 16, 32, 64, 128, 250, 475, 860};
    const static uint8_t len = 2;
    uint8_t data[2];
    esp_err_t err;
    bool tmp; // temporary bool for reading from queue

    if (ads->rdy_pin.in_use) {
        gpio_isr_handler_add(ads->rdy_pin.pin, gpio_isr_handler, (void *)ads->rdy_pin.gpio_evt_queue);
        xQueueReset(ads->rdy_pin.gpio_evt_queue);
    }
    // see if we need to send configuration data
    if ((ads->config.bit.MODE == ADS1115_MODE_SINGLE) || (ads->changed)) { // if it's single-ended or a setting changed
        err = ads1115_write_register(ads, ADS1115_CONFIG_REGISTER_ADDR, ads->config.reg);
        if (err) {
            ESP_LOGE(TAG, "could not write to device: %s", esp_err_to_name(err));
            if (ads->rdy_pin.in_use) {
                gpio_isr_handler_remove(ads->rdy_pin.pin);
                xQueueReset(ads->rdy_pin.gpio_evt_queue);
            }
            return 0;
        }
        ads->changed = 0; // say that the data is unchanged now
    }

    if (ads->rdy_pin.in_use) {
        xQueueReceive(ads->rdy_pin.gpio_evt_queue, &tmp, portMAX_DELAY);
        gpio_isr_handler_remove(ads->rdy_pin.pin);
    } else {
        // wait for 1 ms longer than the sampling rate, plus a little bit for rounding
        vTaskDelay((((1000 / sps[ads->config.bit.DR]) + 1) / portTICK_PERIOD_MS) + 1);
    }

    err = ads1115_read_register(ads, ADS1115_CONVERSION_REGISTER_ADDR, data, len);
    if (err) {
        ESP_LOGE(TAG, "could not read from device: %s", esp_err_to_name(err));
        return 0;
    }
    return ((uint16_t)data[0] << 8) | (uint16_t)data[1];
}

double ads1115_get_voltage(ads1115_t *ads) {
    const double fsr[] = {6.144, 4.096, 2.048, 1.024, 0.512, 0.256};
    const int16_t bits = (1L << 15) - 1;
    int16_t raw;

    raw = ads1115_get_raw(ads);
    return (double)raw * fsr[ads->config.bit.PGA] / (double)bits;
}
