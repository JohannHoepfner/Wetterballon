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

    if (context == NULL || context->intercom == NULL || context->body == NULL || context->body_size == 0) {
        ESP_LOGE(TAG, "Invalid intercom task context");
        if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
            context->status_indicator->set_status(context->status_indicator, UNRECOVERABLE_ERROR);
        }
        vTaskDelete(NULL);
        return;
    }

    if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
        context->status_indicator->set_status(context->status_indicator, INITIALIZING);
    }

    int backoff_sec = 5;
    while (context->intercom->init() != ESP_OK) {
        ESP_LOGE(TAG, "Intercom initialization failed, retrying in %d s", backoff_sec);
        vTaskDelay(pdMS_TO_TICKS(backoff_sec * 1000));
        backoff_sec = backoff_sec < 300 ? backoff_sec * 2 : 300;
    }
    ESP_LOGI(TAG, "Intercom ready");

    size_t prev_lines_read = 0;
    size_t lines_read = 0;
    while (true) {
        size_t body_len = 0;
        esp_err_t read_err = ESP_OK;

        if (context->mode == INTERCOM_TASK_MODE_TELEMETRY) {
            if (context->source.status == NULL || context->source.status->mutex == NULL ||
                context->source.status->value == NULL) {
                ESP_LOGE(TAG, "Invalid intercom status context");
                if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
                    context->status_indicator->set_status(context->status_indicator, UNRECOVERABLE_ERROR);
                }
                vTaskDelete(NULL);
                return;
            }
            xSemaphoreTake(context->source.status->mutex, portMAX_DELAY);
            body_len = strnlen(context->source.status->value, context->body_size - 1);
            memcpy(context->body, context->source.status->value, body_len);
            context->body[body_len] = '\0';
            xSemaphoreGive(context->source.status->mutex);
        } else if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
            Store *store = context->source.source_store.store;

            if (store == NULL || store->read_lines == NULL) {
                ESP_LOGE(TAG, "Invalid intercom SD-card context");
                if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
                    context->status_indicator->set_status(context->status_indicator, UNRECOVERABLE_ERROR);
                }
                vTaskDelete(NULL);
                return;
            }

            ESP_LOGI(TAG, "Reading %zu lines from SD card for intercom send",
                     context->source.source_store.lines_per_send);
            read_err = store->read_lines(context->source.source_store.lines_per_send, context->body, context->body_size,
                                         &lines_read);
            body_len = strnlen(context->body, context->body_size);
        } else {
            ESP_LOGE(TAG, "Unknown intercom task mode");
            if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
                context->status_indicator->set_status(context->status_indicator, UNRECOVERABLE_ERROR);
            }
            vTaskDelete(NULL);
            return;
        }

        if (read_err != ESP_OK) {
            if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
                context->status_indicator->set_status(context->status_indicator, ERROR);
            }
            ESP_LOGE(TAG, "Failed to read intercom data: %s", esp_err_to_name(read_err));
        }

        ESP_LOGI(TAG, "Sending intercom data: %zu bytes, %zu lines", body_len, lines_read);
        esp_err_t send_err = context->intercom->send(context->body, body_len);
        if (send_err != ESP_OK) {
            if (context->status_indicator != NULL && context->status_indicator->set_status != NULL) {
                context->status_indicator->set_status(context->status_indicator, ERROR);
            }
            ESP_LOGE(TAG, "Sending failed");

            if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
                lines_read = prev_lines_read;
            }
        } else {
            if (context->mode == INTERCOM_TASK_MODE_READ_FROM_STORE) {
                ESP_LOGI(TAG, "Sent lines %zu to %zu from SD card via intercom", prev_lines_read, lines_read);

                prev_lines_read = lines_read;
            }
        }

        vTaskDelay(context->send_interval);
    }
}
