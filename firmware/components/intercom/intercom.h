#pragma once

#include <esp_err.h>
#include <stddef.h>

struct intercom {
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);
    esp_err_t (*send)(char *buf, size_t buflen);
};
