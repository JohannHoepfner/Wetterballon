#include "../status_indicator.h"
#include "status_led.h"

#include "led_strip.h"

led_strip_handle_t led_strip;

StatusIndicator status_led = {
    .init = status_led_init,
    .set_status = status_led_set,
};

esp_err_t status_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
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

    err = led_strip_clear(led_strip);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

esp_err_t status_led_set(Status status) {
    struct color color;

    switch (status) {
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
    return ESP_OK;
}
