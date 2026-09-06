/*
 * Modem email POC.
 *
 * Aggregates messages received over the intracom databus (ESPNOW) and periodically
 * emails the accumulated batch out via the SIM7600 modem. Queued messages are only
 * dropped from the batch once a send actually succeeds.
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "databus.h"
#include "esp_log.h"
#include "log_store.h"
#include "neo_m8.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sensor_task.h"

static const char *TAG = "modem_email_poc";

#define AGG_BUF_SIZE 4096

static SemaphoreHandle_t s_agg_mutex;
static char s_agg_buf[AGG_BUF_SIZE];
static size_t s_agg_len = 0;

static void aggregate_message(const char *label, uint64_t send_time, const char *text, size_t text_maxlen) {
    if (xSemaphoreTake(s_agg_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Could not lock aggregate buffer, dropping message");
        return;
    }
    int n = snprintf(s_agg_buf + s_agg_len, sizeof(s_agg_buf) - s_agg_len, "%s [%" PRIu64 "] %.*s\r\n", label,
                     send_time, (int)text_maxlen, text);
    if (n > 0 && (size_t)n < sizeof(s_agg_buf) - s_agg_len) {
        s_agg_len += (size_t)n;
    } else {
        ESP_LOGW(TAG, "Aggregate buffer full, dropping message");
    }
    xSemaphoreGive(s_agg_mutex);
}

static void on_databus_data(struct databus_message *msg) {
    aggregate_message("DATA", msg->send_time, msg->data.message, sizeof(msg->data.message));
}

static void on_databus_log(struct databus_message *msg) {
    aggregate_message("LOG", msg->send_time, msg->log.message, sizeof(msg->log.message));
}

void app_main(void) {
    s_agg_mutex = xSemaphoreCreateMutex();

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    Intracom *intracom = &databus;
    Store *store = &log_store;

    ESP_ERROR_CHECK(intracom->init());
    ESP_ERROR_CHECK(intracom->register_recv_callback(DATABUS_MSG_TYPE_DAT, on_databus_data));
    ESP_ERROR_CHECK(intracom->register_recv_callback(DATABUS_MSG_TYPE_LOG, on_databus_log));
    ESP_ERROR_CHECK(store->init());

    // BaseType_t task_created = xTaskCreate(send_task, "sim_modem_send", 4096, NULL, 5, NULL);
    // if (task_created != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create modem send task");
    // }

    Sensor *sensor_gps = &neo_m8;

    static SensorSchedule schedules[] = {
        {"gps", NULL, pdMS_TO_TICKS(3000)},
    };
    static SensorTaskContext task_contexts[sizeof(schedules) / sizeof(schedules[0])];

    schedules[0].sensor = sensor_gps;

    SemaphoreHandle_t output_mutex = xSemaphoreCreateMutex();
    if (output_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor output mutex");
        return;
    }

    for (size_t i = 0; i < sizeof(schedules) / sizeof(schedules[0]); ++i) {
        task_contexts[i] = (SensorTaskContext){
            .schedule = &schedules[i],
            .intracom = intracom,
            .store = store,
            .output_mutex = output_mutex,
        };
        BaseType_t task_created = xTaskCreate(sensor_task, "sensor_read", 4096, &task_contexts[i], 5, NULL);
        if (task_created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task for sensor %zu", i);
        }
    }
}
