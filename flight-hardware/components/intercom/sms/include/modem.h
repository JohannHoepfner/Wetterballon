#pragma once

#include "esp_modem_c_api_types.h"

esp_err_t modem_start(esp_modem_dce_t **dce_out);
esp_err_t get_signal_quality(esp_modem_dce_t *dce, int *rssi, int *ber);
esp_err_t modem_reset(esp_modem_dce_t *dce);
esp_err_t modem_stop(esp_modem_dce_t *dce);
esp_err_t modem_send_sms(esp_modem_dce_t *dce, char *telno, char *msg);
esp_err_t modem_get_gps_raw(esp_modem_dce_t *dce, char *buf, size_t buf_len);
