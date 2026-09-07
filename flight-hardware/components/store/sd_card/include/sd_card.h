#pragma once

#include "../../store.h"

#include "esp_err.h"

esp_err_t sd_card_init(void);
esp_err_t sd_card_deinit(void);
esp_err_t sd_card_write_data(time_t time, char *msg_str);
esp_err_t sd_card_read_lines(size_t max_lines, char *buffer, size_t buffer_size, size_t *lines_read);
esp_err_t sd_card_acknowledge_lines(size_t lines_read);

extern Store sd_card;
