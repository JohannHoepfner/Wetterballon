#pragma once

#include "../../status_indicator.h"

#include <esp_err.h>

#define BLINK_GPIO 8

struct color {
    int r;
    int g;
    int b;
};

#define RED (struct color){25, 0, 0}
#define GREEN (struct color){0, 25, 0}
#define BLUE (struct color){0, 0, 25}
#define YELLOW (struct color){25, 25, 0}

esp_err_t status_led_init(void);
esp_err_t status_led_set(Status status);

extern StatusIndicator status_led;
