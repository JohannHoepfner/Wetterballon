#pragma once

#include <esp_err.h>
#include <time.h>

struct store {
    esp_err_t (*init)(struct store *self);
    esp_err_t (*reinit)(struct store *self);
    esp_err_t (*deinit)(struct store *self);
    esp_err_t (*save)(struct store *self, time_t time, const char *msg_str);
    esp_err_t (*read_lines)(struct store *, size_t max_lines, char *buffer, size_t buffer_size, size_t *lines_read);
};
