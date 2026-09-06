#pragma once

#include "../../store.h"

#include "esp_err.h"

esp_err_t sd_card_init(void);
esp_err_t sd_card_deinit(void);
esp_err_t sd_card_write_data(time_t time, char *msg_str);

extern Store sd_card;
