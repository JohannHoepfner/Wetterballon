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
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        size_t lines_read = 0;
        size_t body_len = 0;
        esp_err_t read_err = ESP_OK;

        if (context->mode == INTERCOM_TASK_MODE_TELEMETRY) {
            if (context->source.status == NULL || context->source.status->mutex == NULL ||
                context->source.status->value == NULL) {
                ESP_LOGE(TAG, "Invalid intercom status context");
                vTaskDelete(NULL);
                return;
            }
            xSemaphoreTake(context->source.status->mutex, portMAX_DELAY);
            body_len = strnlen(context->source.status->value, context->body_size - 1);
            memcpy(context->body, context->source.status->value, body_len);
            context->body[body_len] = '\0';
            xSemaphoreGive(context->source.status->mutex);
        } else if (context->mode == INTERCOM_TASK_MODE_SD_CARD) {
            if (context->source.sd_card.store == NULL || context->source.sd_card.store->read_lines == NULL ||
                context->source.sd_card.store->acknowledge_lines == NULL || context->source.sd_card.lines_per_send == 0) {
                ESP_LOGE(TAG, "Invalid intercom SD-card context");
                vTaskDelete(NULL);
                return;
            }
            read_err = context->source.sd_card.store->read_lines(
                context->source.sd_card.lines_per_send, context->body, context->body_size, &lines_read);
            if (read_err == ESP_OK && lines_read == 0) {
                body_len = (size_t)snprintf(context->body, context->body_size, "no new data\r\n");
            } else {
                body_len = strlen(context->body);
            }
        } else {
            ESP_LOGE(TAG, "Unknown intercom task mode");
            vTaskDelete(NULL);
            return;
        }

        if (read_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read intercom data: %s", esp_err_to_name(read_err));
        }
        esp_err_t err = read_err == ESP_OK ? context->intercom->send(context->body, body_len) : read_err;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Sending email failed, will retry next cycle (messages stay queued)");
        } else if (context->mode == INTERCOM_TASK_MODE_SD_CARD && lines_read > 0) {
            err = context->source.sd_card.store->acknowledge_lines(lines_read);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to advance SD-card read position: %s", esp_err_to_name(err));
            }
        }

        vTaskDelay(context->send_interval);
    }
}
