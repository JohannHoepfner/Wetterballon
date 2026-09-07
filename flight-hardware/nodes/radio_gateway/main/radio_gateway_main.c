#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_led.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "databus.h"
#include "intercom_task.h"
#include "mock_store.h"
#include "neo_m8.h"
#include "radio.h"
#include "sensor_task.h"

static const char *TAG = "node/radio_gateway";

#define TELEMETRY_BODY_SIZE 256

typedef struct {
    bool valid_time;

    int hour;
    int minute;
    float second;

    double latitude;
    double longitude;
    float altitude;

    float temperature;
} TelemetryContext;

static TelemetryContext *s_telemetry_context;
static IntercomTaskStatus s_telemetry_status;

static char s_telemetry_status_value[TELEMETRY_BODY_SIZE];
static char s_intercom_body[TELEMETRY_BODY_SIZE];

static void format_telemetry_body(const TelemetryContext *context, char *buffer, size_t buffer_size) {
    int hour = context->hour;
    int minute = context->minute;
    float second = context->second;

    // if no time data, use now
    if (!context->valid_time) {
        time_t now = time(NULL);
        hour = localtime(&now)->tm_hour;
        minute = localtime(&now)->tm_min;
        second = localtime(&now)->tm_sec;
    }

    snprintf(buffer, buffer_size, "%02d%02d%02.0f, ,%.4f,%.4f,%05.0fm,%.0f°C", hour, minute, second, context->latitude,
             context->longitude, context->altitude, context->temperature);
}

static void on_databus_data(struct databus_message *message) {
    float new_temperature = 0.0f;

    // Read temp from databus (sens_misc)
    const char *temperature_start = strstr(message->data.message, "t1=");
    if (temperature_start != NULL) {
        new_temperature = strtof(temperature_start + 3, NULL);
    }

    if (s_telemetry_context == NULL) {
        return;
    }

    xSemaphoreTake(s_telemetry_status.mutex, portMAX_DELAY);

    if (new_temperature != s_telemetry_context->temperature) {
        s_telemetry_context->temperature = new_temperature;
    }

    format_telemetry_body(s_telemetry_context, s_telemetry_status.value, s_telemetry_status.value_size);

    xSemaphoreGive(s_telemetry_status.mutex);
}

void app_main(void) {
    // Initialize NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    // Initialize databus
    ESP_ERROR_CHECK(databus.init(NODE_ID_RADIO_GATEWAY));

    // Initialize central adapters
    Intercom *intercom = &radio;
    ESP_ERROR_CHECK(radio.init());
    StatusIndicator *status_indicator = &esp_led;
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));
    Store *store = &mock_store;
    ESP_ERROR_CHECK(store->init());

    // Initialize telemetry status -> this is sent periodically via intercom_task (and broadcasted via radio)
    s_telemetry_status.mutex = xSemaphoreCreateMutex();
    if (s_telemetry_status.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry status mutex");
        return;
    }
    s_telemetry_status.value = s_telemetry_status_value;
    s_telemetry_status.value_size = sizeof(s_telemetry_status_value);
    static TelemetryContext telemetry_context = {
        .valid_time = false,
        .hour = 0,
        .minute = 0,
        .second = 0.0f,
        .latitude = 0.0,
        .longitude = 0.0,
        .altitude = 0.0f,
        .temperature = 0.0f,
    };
    s_telemetry_context = &telemetry_context;

    // Hook up databus receive callback to update telemetry status
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));

    // Set up intercom task to send telemetry status periodically
    static IntercomTaskContext intercom_context;
    intercom_context = (IntercomTaskContext){
        .mode = INTERCOM_TASK_STATUS,
        .intercom = intercom,
        .body = s_intercom_body,
        .body_size = sizeof(s_intercom_body),
        .source.status = &s_telemetry_status,
        .send_interval = pdMS_TO_TICKS(5000),
    };
    BaseType_t intercom_task_created =
        xTaskCreate(intercom_task, "radio_send_telemetry", 4096, &intercom_context, 5, NULL);
    if (intercom_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create modem send task");
        status_indicator->set_status(status_indicator, UNRECOVERABLE_ERROR);
        return;
    }

    // Initialize sensor tasks
    Sensor *sensor_gps = &neo_m8;
    static SensorSchedule schedules[] = {
        {"gps", NULL, NULL, pdMS_TO_TICKS(3000)},
    };
    schedules[0].status_indicator = status_indicator;
    schedules[0].sensor = sensor_gps;
    static SensorTaskContext sensor_task_contexts[sizeof(schedules) / sizeof(schedules[0])];
    esp_err_t err = start_sensor_tasks(schedules, sensor_task_contexts, sizeof(schedules) / sizeof(schedules[0]), store,
                                       status_indicator);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start sensor tasks: %s (0x%x)", esp_err_to_name(err), err);
        status_indicator->set_status(status_indicator, UNRECOVERABLE_ERROR);
        return;
    }
}
