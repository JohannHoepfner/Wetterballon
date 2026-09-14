#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "databus.h"
#include "intercom_task.h"
#include "mock_store.h"
#include "neo_m8.h"
#include "radio.h"
#include "sensor_task.h"

static const char *TAG = "node/radio_gateway";

Intercom *intercom = &radio;
Store *store = &mock_store;
StatusIndicator *status_indicator = &esp_led;
Sensor *sensor_gps = &neo_m8;

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
static char s_telemetry_body[TELEMETRY_BODY_SIZE];
static char s_explanation_body[TELEMETRY_BODY_SIZE];
static char s_greet_emil_body[TELEMETRY_BODY_SIZE];

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

    snprintf(buffer, buffer_size, "%02d%02d%02.0f %02.4f,%02.4f,%05.0f,%c%02.0f", hour, minute, second,
             context->latitude, context->longitude, context->altitude, (context->temperature >= 0) ? '0' : '-',
             fabs(context->temperature));
}

static void on_gps_data(const char *gps_data) {
    if (gps_data == NULL || s_telemetry_context == NULL || strstr(gps_data, "invalid") != NULL) {
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_WARNING);

        if (gps_data != NULL && strstr(gps_data, "utc=00:00:00.0") == NULL) {
            int new_hour, new_minute;
            float new_second;
            int parsed = sscanf(gps_data, "lat=%*f,lon=%*f,alt=%*f,utc=%d:%d:%f", &new_hour, &new_minute, &new_second);
            if (parsed == 3) {
                time_t new_time = time(NULL);
                struct tm new_tm = *localtime(&new_time);
                new_tm.tm_hour = new_hour;
                new_tm.tm_min = new_minute;
                new_tm.tm_sec = (int)new_second;
                new_time = mktime(&new_tm);
                struct timeval tv = {.tv_sec = new_time, .tv_usec = 0};
                settimeofday(&tv, NULL);
            }
        }

        status_indicator->set_status(status_indicator, STATUS_INDICATOR_WARNING);
        return;
    }

    double new_latitude, new_longitude;
    float new_altitude;
    int new_hour, new_minute;
    float new_second;

    ESP_LOGI(TAG, "gps str: %s", gps_data);
    int parsed = sscanf(gps_data, "lat=%lf,lon=%lf,alt=%f,utc=%d:%d:%f", &new_latitude, &new_longitude, &new_altitude,
                        &new_hour, &new_minute, &new_second);

    if (parsed != 6) {
        ESP_LOGW(TAG, "Failed to parse GPS data: %s", gps_data);
        return;
    }

    if (new_latitude < -90.0 || new_latitude > 90.0 || new_longitude < -180.0 || new_longitude > 180.0 ||
        new_hour < 0 || new_hour > 23 || new_minute < 0 || new_minute > 59 || new_second < 0.0f ||
        new_second >= 60.0f) {
        ESP_LOGW(TAG, "Invalid GPS values: %s", gps_data);
        return;
    }

    xSemaphoreTake(s_telemetry_status.mutex, portMAX_DELAY);

    s_telemetry_context->latitude = new_latitude;
    s_telemetry_context->longitude = new_longitude;
    s_telemetry_context->altitude = new_altitude;
    s_telemetry_context->hour = new_hour;
    s_telemetry_context->minute = new_minute;
    s_telemetry_context->second = new_second;

    s_telemetry_context->valid_time = true;

    format_telemetry_body(s_telemetry_context, s_telemetry_status.value, s_telemetry_status.value_size);

    xSemaphoreGive(s_telemetry_status.mutex);

    return;
}

static void on_databus_data(struct databus_message *message) {
    if (message == NULL) {
        ESP_LOGE(TAG, "Invalid databus message");
        return;
    }

    // Also save the message to the store if available
    if (store != NULL) {
        time_t send_time = message->send_time;
        char *msg_str = message->DATA_content.message;
        esp_err_t err = store->save(send_time, msg_str);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save data to store: %s (0x%x)", esp_err_to_name(err), err);
        }
    }

    // Filter out messages, that can't be used for telemetry status update
    if (message->node_id != NODE_ID_SENS_MISC) {
        return;
    }

    float new_temperature = 0.0f;

    // Read temp from databus (sens_misc)
    const char *temperature_start = strstr(message->DATA_content.message, "t1=");
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

    return;
}

static IntercomTaskContext intercom_context_telemetry;
static IntercomTaskContext intercom_context_explanation;
static IntercomTaskContext intercom_context_greet_emil;
static SemaphoreHandle_t s_intercom_send_mutex;

static void on_databus_kill_radio(struct databus_message *message) {
    ESP_LOGE(TAG, "radio star was murdered.");
    intercom_context_telemetry.intercom = NULL;
    intercom_context_explanation.intercom = NULL;
    intercom_context_greet_emil.intercom = NULL;
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
    ESP_ERROR_CHECK(status_indicator->init(status_indicator));
    ESP_ERROR_CHECK(store->init());

    // Initialize radio
    ESP_ERROR_CHECK(intercom->init());

    s_intercom_send_mutex = xSemaphoreCreateMutex();
    if (s_intercom_send_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create intercom send mutex");
        return;
    }

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
    xSemaphoreTake(s_telemetry_status.mutex, portMAX_DELAY);
    format_telemetry_body(s_telemetry_context, s_telemetry_status.value, s_telemetry_status.value_size);
    xSemaphoreGive(s_telemetry_status.mutex);

    // Hook up databus receive callback to update telemetry status
    ESP_ERROR_CHECK(sensor_gps->on_receive(on_gps_data));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_DATA, on_databus_data));
    ESP_ERROR_CHECK(databus.on_receive(DATABUS_MSG_TYPE_KILL_RADIO, on_databus_kill_radio));

    // Set up intercom task to send last telemetry status periodically via intercom
    intercom_context_telemetry.mode = INTERCOM_TASK_MODE_TELEMETRY;
    intercom_context_telemetry.intercom = intercom;
    intercom_context_telemetry.body = s_telemetry_body;
    intercom_context_telemetry.body_size = sizeof(s_telemetry_body);
    intercom_context_telemetry.send_mutex = s_intercom_send_mutex;
    intercom_context_telemetry.source.status = &s_telemetry_status;
    intercom_context_telemetry.send_interval = pdMS_TO_TICKS(60000); // Send every 60 seconds
    intercom_context_telemetry.status_indicator = status_indicator;
    BaseType_t intercom_task_created =
        xTaskCreate(intercom_task, "radio_send_telemetry", 4096, &intercom_context_telemetry, 5, NULL);
    if (intercom_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create intercom task");
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_INTERCOM_ERROR);
        return;
    }

    // Set up intercom task to send explanation message periodically via intercom
    char *explanation_message =
        "FORMAT IS TIME KOORD-LAT,KOORD-LONG,TEMP CRC. MORE AT DA0FRA.ALTAFRANER.DE. PRPT VIA EMAIL WELCOME";
    intercom_context_explanation.mode = INTERCOM_TASK_MODE_TEXT;
    intercom_context_explanation.intercom = intercom;
    intercom_context_explanation.body = s_explanation_body;
    intercom_context_explanation.body_size = sizeof(s_explanation_body);
    intercom_context_explanation.send_mutex = s_intercom_send_mutex;
    intercom_context_explanation.source.source_text.text = explanation_message;
    intercom_context_explanation.send_interval = pdMS_TO_TICKS(300000); // Send every 5 minutes
    intercom_context_explanation.status_indicator = status_indicator;
    BaseType_t intercom_explanation_task_created =
        xTaskCreate(intercom_task, "radio_send_explanation", 4096, &intercom_context_explanation, 5, NULL);
    if (intercom_explanation_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create intercom explanation task");
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_INTERCOM_ERROR);
        return;
    }

    // Set up intercom task to send greeting message periodically via intercom
    char *greet_emil_message = "GREETINGS TO DO1ESL";
    intercom_context_greet_emil.mode = INTERCOM_TASK_MODE_TEXT;
    intercom_context_greet_emil.intercom = intercom;
    intercom_context_greet_emil.body = s_greet_emil_body;
    intercom_context_greet_emil.body_size = sizeof(s_greet_emil_body);
    intercom_context_greet_emil.send_mutex = s_intercom_send_mutex;
    intercom_context_greet_emil.source.source_text.text = greet_emil_message;
    intercom_context_greet_emil.send_interval = pdMS_TO_TICKS(600000); // Send every 10 minutes
    intercom_context_greet_emil.status_indicator = status_indicator;
    BaseType_t intercom_greet_emil_task_created =
        xTaskCreate(intercom_task, "radio_send_greet_emil", 4096, &intercom_context_greet_emil, 5, NULL);
    if (intercom_greet_emil_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create intercom greet emil task");
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_INTERCOM_ERROR);
        return;
    }

    // Initialize sensor tasks
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
        status_indicator->set_status(status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
        return;
    }

    // Periodically send timesync messages via databus (to sync all nodes)
    const TickType_t timesync_interval = pdMS_TO_TICKS(60000);
    while (true) {
        time_t now = time(NULL);
        databus.send_timesync(now);
        vTaskDelay(timesync_interval);
    }
}
