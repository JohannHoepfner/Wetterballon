#include "databus.h"
#include "uv_pt100.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <nvs_flash.h>

static const char *TAG = "sens_uv";

void app_main(void) {
    ESP_LOGI(TAG, "init");

    Intracom *intracom_databus = &databus;

    Sensor *uv_1 = &uv_pt100_sensor;
    uv_1->ctx = malloc(sizeof(UV_PT100_Sensor));
    ((UV_PT100_Sensor *)uv_1->ctx)->photo_num = 1;

    Sensor *uv_2 = &uv_pt100_sensor;
    uv_2->ctx = malloc(sizeof(UV_PT100_Sensor));
    ((UV_PT100_Sensor *)uv_2->ctx)->photo_num = 2;

    Sensor *uv_3 = &uv_pt100_sensor;
    uv_3->ctx = malloc(sizeof(UV_PT100_Sensor));
    ((UV_PT100_Sensor *)uv_3->ctx)->photo_num = 3;

    Sensor *pt100 = &uv_pt100_sensor;
    pt100->ctx = malloc(sizeof(UV_PT100_Sensor));
    ((UV_PT100_Sensor *)pt100->ctx)->type = PT100_SENSOR;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(intracom_databus->init());

    ESP_ERROR_CHECK(uv_1->init(uv_1));
    ESP_ERROR_CHECK(uv_2->init(uv_2));
    ESP_ERROR_CHECK(uv_3->init(uv_3));
    ESP_ERROR_CHECK(pt100->init(pt100));

    while (true) {
        double cels = pt100->read(pt100);
        ESP_LOGI(TAG, "%lf°C", cels);
        double volt1 = uv_1->read(uv_1);
        double volt2 = uv_2->read(uv_2);
        double volt3 = uv_3->read(uv_3);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt1);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt2);
        ESP_LOGI(TAG, "uv: %d, diff: %lfV", 1, volt3);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return;
}
