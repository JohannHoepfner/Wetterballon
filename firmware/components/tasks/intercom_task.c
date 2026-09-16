#include "intercom_task.h"

#include <freertos/task.h>

#include <esp_err.h>
#include <esp_log.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "task/intercom_task";

static void set_status(void *context, const char *value) {
    IntercomTaskStatus *status = (IntercomTaskStatus *)context;

    if (status == NULL || status->mutex == NULL || status->value == NULL || status->value_size == 0 || value == NULL) {
        return;
    }
    xSemaphoreTake(status->mutex, portMAX_DELAY);
    snprintf(status->value, status->value_size, "%s", value);
    xSemaphoreGive(status->mutex);
}

IntercomStatusHandler INTERCOM_TASK_MODE_TELEMETRY_handler(IntercomTaskContext *context) {
    return (IntercomStatusHandler){
        .set = set_status,
        .context = context == NULL ? NULL : context->source.status,
    };
}

void intercom_task(void *arg) {
    const IntercomTaskContext *context = (const IntercomTaskContext *)arg;

    if (context == NULL || context->intercom == NULL || context->body == NULL || context->body_size == 0 ||
        context->status_indicator == NULL || context->status_indicator->set_status == NULL) {
        ESP_LOGE(TAG, "Invalid intercom task context");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Intercom task started in mode %d", context->mode);
    context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_INITIALIZING);

    size_t prev_lines_read = 0;
    size_t lines_read = 0;
    while (true) {
        size_t body_len = 0;
        esp_err_t read_err = ESP_OK;

        if (context->mode == INTERCOM_TASK_MODE_TELEMETRY) {
            if (context->source.status == NULL || context->source.status->mutex == NULL ||
                context->source.status->value == NULL || context->intercom == NULL) {
                ESP_LOGE(TAG, "Invalid intercom status context");
                context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
                vTaskDelete(NULL);
                return;
            }
            xSemaphoreTake(context->source.status->mutex, portMAX_DELAY);
            body_len = strnlen(context->source.status->value, context->body_size - 1);
            memcpy(context->body, context->source.status->value, body_len);
            context->body[body_len] = '\0';
            xSemaphoreGive(context->source.status->mutex);
        } else if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
            struct store *store = context->source.source_store.store;

            if (store == NULL || store->read_lines == NULL) {
                ESP_LOGE(TAG, "Invalid intercom SD-card context");
                context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
                vTaskDelete(NULL);
                return;
            }

            ESP_LOGI(TAG, "Reading %zu lines from SD card for intercom send",
                     context->source.source_store.lines_per_send);
            xSemaphoreTake(context->source.status->mutex, portMAX_DELAY);
            read_err = store->read_lines(store, context->source.source_store.lines_per_send, context->body,
                                         context->body_size, &lines_read);
            body_len = strnlen(context->body, context->body_size);
            xSemaphoreGive(context->source.status->mutex);
        } else if (context->mode == INTERCOM_TASK_MODE_TEXT) {
            if (context->source.source_text.text == NULL) {
                ESP_LOGE(TAG, "Invalid intercom text context");
                context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
                vTaskDelete(NULL);
                return;
            }

            body_len = strnlen(context->source.source_text.text, context->body_size - 1);
            memcpy(context->body, context->source.source_text.text, body_len);
            context->body[body_len] = '\0';
        } else {
            ESP_LOGE(TAG, "Unknown intercom task mode");
            context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_SENSOR_ERROR);
            vTaskDelete(NULL);
            return;
        }

        if (read_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read intercom data: %s", esp_err_to_name(read_err));
            context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_ERROR);
        } else {
            // Only send if read was successful

            ESP_LOGI(TAG, "Sending intercom data: %zu bytes, %zu lines", body_len, lines_read);
            if (context->send_mutex != NULL) {
                xSemaphoreTake(context->send_mutex, portMAX_DELAY);
            }
            esp_err_t send_err = context->intercom->send(context->body, body_len);
            if (context->send_mutex != NULL) {
                xSemaphoreGive(context->send_mutex);
            }
            if (send_err != ESP_OK) {
                ESP_LOGE(TAG, "Sending failed");
                context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_ERROR);

                if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
                    lines_read = prev_lines_read;
                }
            } else {
                if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
                    prev_lines_read = lines_read;

                    ESP_LOGI(TAG, "Sent lines %zu to %zu from SD card via intercom", prev_lines_read, lines_read);
                    context->status_indicator->set_status(context->status_indicator, STATUS_INDICATOR_OK);
                }
            }
        }

        vTaskDelay(context->send_interval);
    }
}
