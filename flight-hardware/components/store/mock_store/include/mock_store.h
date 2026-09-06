#pragma once

#include "../../store.h"

#include "esp_err.h"

esp_err_t mock_store_init(void);
esp_err_t mock_store_deinit(void);
esp_err_t mock_store_save(time_t time, char *msg_str);

extern Store mock_store;
