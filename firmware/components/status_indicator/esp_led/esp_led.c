#include "esp_led.h"

#include "../status_indicator.h"

#include "driver/gpio.h"
#include "led_strip.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "status_indicator/esp_led";

typedef struct EspLedContext {
    Status status;
} EspLedContext;

EspLedContext esp_led_context;

led_strip_handle_t led_strip;
static SemaphoreHandle_t esp_led_mutex;

StatusIndicator esp_led = {
    .ctx = &esp_led_context,
    .init = esp_led_init,
    .set_status = esp_led_set,
};

esp_err_t esp_led_init(StatusIndicator *self) {
    esp_led_mutex = xSemaphoreCreateMutex();
    if (esp_led_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = GPIO_NUM_8,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err;

    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_led_set(self, STATUS_INDICATOR_INITIALIZING);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "LED initialized successfully");

    return ESP_OK;
}

esp_err_t esp_led_set(StatusIndicator *self, Status status) {
    if (esp_led_mutex == NULL || xSemaphoreTake(esp_led_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }

    // Make certain statuses persistent
    if (((EspLedContext *)self->ctx)->status == STATUS_INDICATOR_SENSOR_ERROR ||
        ((EspLedContext *)self->ctx)->status == STATUS_INDICATOR_SD_CARD_ERROR ||
        ((EspLedContext *)self->ctx)->status == STATUS_INDICATOR_INTERCOM_ERROR) {
        xSemaphoreGive(esp_led_mutex);
        return ESP_OK;
    }

    struct color color;
    switch (status) {
    case STATUS_INDICATOR_INITIALIZING:
        color = BLUE;
        break;
    case STATUS_INDICATOR_SD_CARD_ERROR:
        color = PINK;
        break;
    case STATUS_INDICATOR_SENSOR_ERROR:
        color = PURPLE;
        break;
    case STATUS_INDICATOR_INTERCOM_ERROR:
        color = PURPLE;
        break;
    case STATUS_INDICATOR_ERROR:
        color = RED;
        break;
    case STATUS_INDICATOR_WARNING:
        color = YELLOW;
        break;
    case STATUS_INDICATOR_OK:
        color = GREEN;
        break;
    default:
        color = RED;
        break;
    }

    esp_err_t err;
    err = led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b);
    if (err != ESP_OK) {
        xSemaphoreGive(esp_led_mutex);
        return err;
    }
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        xSemaphoreGive(esp_led_mutex);
        return err;
    }

    ((EspLedContext *)self->ctx)->status = status;
    xSemaphoreGive(esp_led_mutex);

    return ESP_OK;
}
