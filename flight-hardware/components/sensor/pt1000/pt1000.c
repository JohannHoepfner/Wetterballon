#include "pt1000.h"

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <stdio.h>

ads1115_t ads; // 0-1 PT1000 | 3 PT1000 (more accurate)

static const char *TAG = "sensor/pt1000";
static char formatted_voltage[48];

Sensor pt1000 = {
    .init = pt1000_init,
    .read = pt1000_read,
};

esp_err_t init_ads(void) {
    i2c_master_bus_config_t config = {
        .sda_io_num = CONFIG_PT1000_I2C_SDA_PIN,
        .scl_io_num = CONFIG_PT1000_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    i2c_master_bus_handle_t handle;
    if (i2c_new_master_bus(&config, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "i2c new master bus FAIL");
        return -1;
    }
    ESP_LOGI(TAG, "i2c new master bus OK");

    i2c_device_config_t dev_config = {
        .device_address = CONFIG_PT1000_I2C_ADC_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 1E6,
    };
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(handle, &dev_config, &dev_handle));

    ads = ads1115_config(dev_handle);
    ads1115_set_mode(&ads, ADS1115_MODE_SINGLE);

    return ESP_OK;
}

double get_voltage(ads1115_t *ads, ads1115_mux_t mux, ads1115_fsr_t fsr) {
    ads1115_set_mux(ads, mux);
    ads1115_set_pga(ads, fsr);
    double volt = ads1115_get_voltage(ads);
    return volt;
}

esp_err_t pt1000_init(void) {


    init_ads();

    ESP_LOGI(TAG, "PT1000 sensor initialized");
    return ESP_OK;
}

char *pt1000_read(void) {
    double differential_voltage = get_voltage(&ads, ADS1115_MUX_0_1, ADS1115_FSR_6_144);
    double a3_voltage = get_voltage(&ads, ADS1115_MUX_3_GND, ADS1115_FSR_6_144);

    ESP_LOGI(TAG, "A0-A1 voltage: %.4f V, A3 voltage: %.4f V", differential_voltage, a3_voltage);

    return pt1000_format(differential_voltage, a3_voltage);
}

double volt_to_temp(double volt) {
    // Formel mittels Regression aus den Messwerten
    double a = 12.8283;
    double b = 64.8344;
    double c = -95.7200;

    double offset = 0.5755;

    return a * volt * volt + b * volt + c - offset;
}

char *pt1000_format(double differential_voltage, double a3_voltage) {
    snprintf(formatted_voltage, sizeof(formatted_voltage), "a0-a1=%.4f,a3=%.4f,t1=%.4f", differential_voltage,
             a3_voltage, volt_to_temp(a3_voltage));
    return formatted_voltage;
}
