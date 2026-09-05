#pragma once

#include "../../intercom.h"

#include "esp_err.h"

esp_err_t sms_init(void);
esp_err_t sms_deinit(void);
esp_err_t sms_send_msg(char *buf, size_t buflen);

extern Intercom sms;
