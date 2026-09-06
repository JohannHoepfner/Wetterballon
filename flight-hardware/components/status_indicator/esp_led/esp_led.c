#include "esp_led.h"

#include "../status_indicator.h"

#include "driver/gpio.h"
#include "led_strip.h"
#include <esp_err.h>
#include <esp_log.h>

static const char *TAG = "status_indicator/esp_led";

typedef struct EspLedContext {
    Status status;
} EspLedContext;

EspLedContext esp_led_context;

led_strip_handle_t led_strip;

StatusIndicator esp_led = {
    .ctx  = &esp_led_context,
    .init = esp_led_init,
    .set_status = esp_led_set,
};

esp_err_t esp_led_init(StatusIndicator *self) {
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
    err = esp_led_set(self, INITIALIZING);
    if (err != ESP_OK) {
        return err;
    }

    esp_led_set(self, OK);

    ESP_LOGI(TAG, "LED initialized successfully");

    return ESP_OK;
}

esp_err_t esp_led_set(StatusIndicator *self, Status status) {
    if (((EspLedContext *)self->ctx)->status == UNRECOVERABLE_ERROR) {
        return ESP_OK;
    }

    struct color color;
    switch (status) {
    case INITIALIZING:
        color = BLUE;
        break;
    case UNRECOVERABLE_ERROR:
        color = PURPLE;
        break;
    case ERROR:
        color = RED;
        break;
    case WARNING:
        color = YELLOW;
        break;
    case OK:
        color = GREEN;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;
    err = led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b);
    if (err != ESP_OK) {
        return err;
    }
    err = led_strip_refresh(led_strip);
    if (err != ESP_OK) {
        return err;
    }

    ((EspLedContext *)self->ctx)->status = status;

    return ESP_OK;
}
