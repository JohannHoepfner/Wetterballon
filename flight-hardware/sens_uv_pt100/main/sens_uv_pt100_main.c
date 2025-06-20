#include "uv_pt100.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>

#include "databus.h"

static const char *TAG = "sens_uv";

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
        double volt1 = get_voltage_photo(1);
        double volt2 = get_voltage_photo(2);
        double volt3 = get_voltage_photo(3);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt1);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt2);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt3);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return;
}
