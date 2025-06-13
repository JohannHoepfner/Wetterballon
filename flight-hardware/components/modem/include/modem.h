#pragma once

#include "esp_modem_c_api_types.h"

#define MAX_HTTP_OUTPUT_BUFFER 2048

esp_err_t modem_start(esp_modem_dce_t **dce_out, esp_netif_t **esp_netif_out);
esp_err_t modem_stop(esp_modem_dce_t *dce, esp_netif_t *esp_netif);

esp_err_t modem_send_sms(esp_modem_dce_t *dce, char *telno, char *msg);

esp_err_t modem_get_gps_raw(esp_modem_dce_t *dce, char *buf, size_t buf_len);
