#include "include/uv_pt100.h"

#include "ads1115.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <math.h>

static const char *TAG = "sens_uv";

ads1115_t ads1; // 0-1 UV 2  | 2-3 UV 3
ads1115_t ads2; // 0-1 PT100 | 2-3 UV 1

esp_err_t init_ads() {
    i2c_master_bus_config_t config = {
        .sda_io_num = CONFIG_UV_PT100_I2C_SDA_PIN,
        .scl_io_num = CONFIG_UV_PT100_I2C_SCL_PIN,
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

    i2c_device_config_t dev_config1 = {
        .device_address = CONFIG_UV_PT100_I2C_ADC_1_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 1E6,
    };
    i2c_device_config_t dev_config2 = {
        .device_address = CONFIG_UV_PT100_I2C_ADC_2_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 1E6,
    };

    i2c_master_dev_handle_t dev_handle1;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(handle, &dev_config1, &dev_handle1));
    i2c_master_dev_handle_t dev_handle2;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(handle, &dev_config2, &dev_handle2));

    ads1 = ads1115_config(dev_handle1);
    ads1115_set_mode(&ads1, ADS1115_MODE_SINGLE);

    ads2 = ads1115_config(dev_handle2);
    ads1115_set_mode(&ads2, ADS1115_MODE_SINGLE);
    return ESP_OK;
}

double get_voltage(ads1115_t *ads, ads1115_mux_t mux, ads1115_fsr_t fsr) {
    ads1115_set_mux(ads, mux);
    ads1115_set_pga(ads, fsr);
    double volt = ads1115_get_voltage(ads);
    return volt;
}

double get_voltage_photo(int photo_num) {
    ads1115_t *ads;
    ads1115_mux_t mux;

    switch (photo_num) {
    case 1:
        ads = &ads2;
        mux = ADS1115_MUX_3_GND;
        break;
    case 2:
        ads = &ads1;
        mux = ADS1115_MUX_0_GND;
        break;
    case 3:
        ads = &ads1;
        mux = ADS1115_MUX_3_GND;
        break;
    default:
        return NAN;
    }

    double volt = get_voltage(ads, mux, ADS1115_FSR_6_144);

    return volt;
}

double pt100_ohm_to_cels(double r_ohm) {
    double A = 3.9083E-3;
    double B = -5.775E-7;
    return (-A + sqrt(pow(A, 2) - 4 * B * (1 - r_ohm / 100))) / (2 * B);
}

double pt100_wheatstone_volt_to_ohms(double u_v) {
    return 104887.132676601 * u_v * u_v - 6686.13419344201 * u_v + 100.0416383514;
}

double pt100_get_temp_cels() {
    double volt = get_voltage(&ads2, ADS1115_MUX_0_1, ADS1115_FSR_0_256);
    double ohms = pt100_wheatstone_volt_to_ohms(-volt);
    double cels = pt100_ohm_to_cels(ohms);
    return cels;
}
