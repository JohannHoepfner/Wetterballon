#pragma once

#include <esp_err.h>
#include <stdbool.h>

void radio_send(void);

esp_err_t radio_init();
esp_err_t radio_send_data(char *buf, size_t buflen);
esp_err_t radio_send_bits(bool *buf, size_t buflen);
