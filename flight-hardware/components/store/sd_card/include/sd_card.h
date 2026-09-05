#pragma once

#include "../../store.h"

#include "esp_err.h"

esp_err_t sdcard_init(void);
esp_err_t sdcard_deinit(void);
esp_err_t write_data(time_t time, char *msg_str);
ssize_t read_databus_messages(struct databus_message *out_message, size_t start, size_t count);

extern Store sd_card;
