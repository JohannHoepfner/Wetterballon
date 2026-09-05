#pragma once

#include "../../store.h"

#include "databus_message.h"
#include "esp_err.h"

esp_err_t sdcard_init();
esp_err_t sdcard_deinit();

esp_err_t save_databus_message(struct databus_message *message);
ssize_t read_databus_messages(struct databus_message *out_message, size_t start, size_t count);

extern Store sdcard;
