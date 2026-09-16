#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "databus.h"
#include "esp_led.h"
#include "esp_log.h"
#include "intercom_task.h"
#include "mock_store.h"
#include "nvs_flash.h"
#include "sd_card.h"
#include "sdkconfig.h"
#include "sim_modem.h"

static const char *TAG = "node/cell_gateway";

struct intercom *intercom = &sim_modem;
static struct store_sd_card store_sd_card;
static struct store *store;
struct status_indicator *status_indicator = &esp_led;

#define TELEMETRY_BODY_SIZE (64 * 1024)
static char s_intercom_body[TELEMETRY_BODY_SIZE];

static void
on_databus_data(struct databus_message *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid databus message");
        return;
    }

    if (store == NULL) {
        ESP_LOGE(TAG, "Store is not initialized");
        return;
    }

    time_t send_time = message->send_time;
    char *msg_str = message->DATA_content.message;

    esp_err_t err = store->save(store, send_time, msg_str);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save data to store: %s (0x%x)", esp_err_to_name(err), err);
    }
}

void
app_main(void) {
    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    // Initialize databus
    ESP_ERROR_CHECK(databus.init(NODE_ID_CELL_GATEWAY));

    // Initialize central adapters
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));

    sd_card_store_create(&store_sd_card, &(struct store_sd_card_cfg){
                                             .pin_mosi = GPIO_NUM_5,
                                             .pin_miso = GPIO_NUM_4,
                                             .pin_clk = GPIO_NUM_6,
                                             .pin_cs = GPIO_NUM_0,
                                         });
    store = &store_sd_card.base;

    esp_err_t store_init_err = store->init(store);
    if (store_init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize store: %s (0x%x)", esp_err_to_name(store_init_err), store_init_err);
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SD_CARD_ERROR);
        ESP_LOGW(TAG, "Continuing without store functionality (using mock store)");
        store = &mock_store;
        ESP_ERROR_CHECK(store->init(store));
    }

    // Initialize sim_modem (intercom)
    int backoff_sec = 5;
    while (intercom->init() != ESP_OK) {
        ESP_LOGE(TAG, "Sim modem initialization failed, retrying in %d s", backoff_sec);
        vTaskDelay(pdMS_TO_TICKS(backoff_sec * 1000));
        backoff_sec = backoff_sec < 300 ? backoff_sec * 2 : 300;
    }
    ESP_LOGI(TAG, "Sim modem ready");

    // Hook up databus receive callback to save messages to store
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_TIMESYNC, on_databus_timesync_default));

    // Set up intercom task to send sd card data via intercom
    static IntercomTaskContext intercom_context;
    intercom_context = (IntercomTaskContext){
        .mode = INTERCOM_TASK_MODE_READ_FROM_STORE,
        .intercom = intercom,
        .body = s_intercom_body,
        .body_size = sizeof(s_intercom_body),
        .source.source_store.store = store,
        .source.source_store.lines_per_send = 500,
        .send_interval = pdMS_TO_TICKS(10000),
        .status_indicator = status_indicator,
    };
    BaseType_t intercom_task_created =
        xTaskCreate(intercom_task, "cell_send_telemetry", 4096, &intercom_context, 5, NULL);
    if (intercom_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create intercom task");
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
        return;
    }
}
