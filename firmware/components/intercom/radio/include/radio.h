#pragma once

#include "../../intercom.h"

#include "esp_err.h"

esp_err_t radio_init(void);
esp_err_t radio_deinit(void);
esp_err_t radio_send_msg(char *buf, size_t buflen);

extern struct intercom radio;
