#pragma once

#include "../../status_indicator.h"

#include <esp_err.h>

struct color {
    int r;
    int g;
    int b;
};

#define RED (struct color){25, 0, 0}
#define GREEN (struct color){0, 25, 0}
#define BLUE (struct color){0, 0, 25}
#define YELLOW (struct color){25, 10, 0}
#define PURPLE (struct color){25, 0, 25}

esp_err_t esp_led_init(StatusIndicator *self);
esp_err_t esp_led_set(StatusIndicator *self, Status status);

extern StatusIndicator esp_led;
