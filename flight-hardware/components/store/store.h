#pragma once

#include <esp_err.h>
#include <time.h>

typedef struct Store {
    esp_err_t (*init)();
    esp_err_t (*reinit)();
    esp_err_t (*deinit)();
    esp_err_t (*save)(time_t time, char *msg_str);
    esp_err_t (*read_lines)(size_t max_lines, char *buffer, size_t buffer_size, size_t *lines_read);
} Store;
