#include "status_led.h"

#include "led_strip.h"

led_strip_handle_t led_strip;

struct color color_mix(struct color a, struct color b, double ratio) {
    ratio = MIN(ratio, 1.);
    ratio = MAX(ratio, 0.);
    return (struct color){
        .r = a.r * ratio + b.r * (1 - ratio),
        .g = a.g * ratio + b.g * (1 - ratio),
        .b = a.b * ratio + b.b * (1 - ratio),
    };
}

esp_err_t status_led_set(struct color color) {
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
