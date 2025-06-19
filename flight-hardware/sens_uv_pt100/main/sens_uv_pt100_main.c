#include "ads1115.h"

#include <driver/i2c_master.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <math.h>
#include <nvs_flash.h>

#include "databus.h"

static const char *TAG = "sens_uv";

double ohm_to_cels(double ohm) {
    double A = 3.9083E-3;
    double B = -5.775E-7;
    return (-A + sqrt(pow(A, 2) - 4 * B * (1 - ohm / 100))) / (2 * B);
}

double volt_to_ohms(double volts) {
    return 104887.132676601 * volts * volts - 6686.13419344201 * volts + 100.0416383514;
}

ads1115_t ads1; // 0-1 UV 2  | 2-3 UV 3
ads1115_t ads2; // 0-1 PT100 | 2-3 UV 1

esp_err_t init_ads() {
    i2c_master_bus_config_t config = {
        .sda_io_num = 6,
        .scl_io_num = 7,
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
        .device_address = 0x48,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 1E6,
    };
    i2c_device_config_t dev_config2 = {
        .device_address = 0x49,
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

double pt100_get_temp_cels() {
    ads1115_set_mux(&ads2, ADS1115_MUX_0_1);
    ads1115_set_pga(&ads2, ADS1115_FSR_0_256);
    double volt = ads1115_get_voltage(&ads2);
    double ohms = volt_to_ohms(-volt);
    double cels = ohm_to_cels(ohms);
    return cels;
}

double get_voltage(ads1115_t *ads, ads1115_mux_t mux, ads1115_fsr_t fsr) {
    ads1115_set_mux(ads, mux);
    ads1115_set_pga(ads, fsr);
    double volt = ads1115_get_voltage(ads);
    return volt;
}

esp_err_t get_voltage_photo(int photo_num) {
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
        return -ESP_ERR_INVALID_ARG;
    }

    double volt = get_voltage(ads, mux, ADS1115_FSR_6_144);
    ESP_LOGI(TAG, "uv: %d, diff: %lfV", photo_num, volt);

    return ESP_OK;
}

void app_main(void) {

    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    databus_wifi_init();
    databus_init();

    init_ads();

    while (true) {
        double cels = pt100_get_temp_cels();
        ESP_LOGI(TAG, "%lf°C", cels);
        ESP_ERROR_CHECK(get_voltage_photo(1));
        ESP_ERROR_CHECK(get_voltage_photo(2));
        ESP_ERROR_CHECK(get_voltage_photo(3));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return;
}
