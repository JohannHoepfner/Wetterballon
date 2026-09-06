#pragma once

#include "../../store.h"

#include "esp_err.h"

esp_err_t log_init(void);
esp_err_t log_deinit(void);
esp_err_t log_data(time_t time, char *msg_str);
ssize_t no_read(struct databus_message *out_message, size_t start, size_t count);

extern Store log_store;
