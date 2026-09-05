#include "sms.h"
#include "databus.h"
#include "sd_card.h"
#include "mock.h"
#include "geiger.h"

#include <freertos/FreeRTOS.h>

#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <stdbool.h>
#include <string.h>

static const char *TAG = "sms_gateway";

time_t now;

void app_main(void)
{
    ESP_LOGI(TAG, "init");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // char *seq = "hello world";
    
    // Intercom *intercom_sms = &sms;
    Intracom *intracom_databus = &databus;
    Store *store_sd_card = &sd_card;

    Sensor *sensor_mock = &mock;
    Sensor *sensor_geiger = &geiger;

    // ESP_ERROR_CHECK(intercom_sms->init());
    ESP_ERROR_CHECK(intracom_databus->init());
    ESP_ERROR_CHECK(store_sd_card->init());
    ESP_ERROR_CHECK(sensor_mock->init(sensor_mock));
    ESP_ERROR_CHECK(sensor_geiger->init(sensor_geiger));

    // List of sensors to read from
    Sensor *sensors[] = {sensor_mock, sensor_geiger};

    while (true) { // TODO: add a way to exit this loop
        esp_err_t err;

        // esp_err_t err = intercom_sms->send(seq, strlen(seq));
        // if (err != ESP_OK) {
        //     ESP_LOGE(TAG, "Failed to send message: %s (0x%x)",
        //              esp_err_to_name(err), err);
        //     continue;
        // }

        // Read sensor data of every sensor and save/send it
        for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); ++i) {
            Sensor *sensor = sensors[i];
            char *sensor_value = sensor->read(sensor);

            // Save data to SD Card
            err = store_sd_card->save(now, sensor_value);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save data to SD card: %s (0x%x)",
                         esp_err_to_name(err), err);
                continue;
            }

            // Send data via intracom
            err = intracom_databus->send_data(now, sensor_value);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send data via intracom: %s (0x%x)",
                         esp_err_to_name(err), err);
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(600));
    }

    // ESP_ERROR_CHECK(intercom_sms->deinit());
    ESP_ERROR_CHECK(store_sd_card->deinit());
}