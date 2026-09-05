#include "modem.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_modem_c_api_types.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "modem";
static esp_netif_t *modem_netif;

#if defined(CONFIG_SIM_GATEWAY_FLOW_CONTROL_NONE)
#define SIM_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_SIM_GATEWAY_FLOW_CONTROL_SW)
#define SIM_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_SIM_GATEWAY_FLOW_CONTROL_HW)
#define SIM_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif

esp_err_t modem_start(esp_modem_dce_t **dce_out) {
    esp_err_t err;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("");

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CONFIG_SIM_GATEWAY_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_SIM_GATEWAY_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_SIM_GATEWAY_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_SIM_GATEWAY_MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = SIM_GATEWAY_FLOW_CONTROL;
    dte_config.uart_config.rx_buffer_size = CONFIG_SIM_GATEWAY_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_SIM_GATEWAY_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_SIM_GATEWAY_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_SIM_GATEWAY_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_SIM_GATEWAY_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_SIM_GATEWAY_MODEM_UART_RX_BUFFER_SIZE / 2;

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_PPP();
    modem_netif = esp_netif_new(&netif_config);
    if (modem_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create esp-modem network interface");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, modem_netif);

    if (dce == NULL) {
        esp_netif_destroy(modem_netif);
        modem_netif = NULL;
        return ESP_FAIL;
    }
    if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        err = esp_modem_set_flow_control(dce, 2, 2);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set the set_flow_control mode");
            return err;
        }
        ESP_LOGI(TAG, "HW set_flow_control OK");
    }

    while (true) {
        ESP_LOGI(TAG, "REBOOTING MODEM");
        gpio_set_direction(CONFIG_SIM_GATEWAY_MODEM_PWK_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_SIM_GATEWAY_MODEM_PWK_PIN, 0);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        gpio_set_level(CONFIG_SIM_GATEWAY_MODEM_PWK_PIN, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "BOOTING MODEM");
        for (int try = 0; try < 30; ++try) {
            ESP_LOGI(TAG, "MODEM not ready. waiting 500ms");
            vTaskDelay(pdMS_TO_TICKS(500));
            if (esp_modem_sync(dce) == ESP_OK) {
                break;
            }
        }

        if (esp_modem_sync(dce) == ESP_OK) {
            break;
        }
    }

    ESP_ERROR_CHECK(esp_modem_sync(dce));

    int rssi, ber;
    err = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return err;
    }

    *dce_out = dce;

    return ESP_OK;
}

esp_err_t get_signal_quality(esp_modem_dce_t *dce, int *rssi, int *ber) {
    esp_err_t err = esp_modem_get_signal_quality(dce, rssi, ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", *rssi, *ber);
    return ESP_OK;
}

esp_err_t modem_reset(esp_modem_dce_t *dce) {
    esp_err_t err = esp_modem_reset(dce);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_reset failed with %d %s", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Modem restarted successfully");
    return ESP_OK;
}

esp_err_t modem_stop(esp_modem_dce_t *dce) {
    esp_modem_destroy(dce);
    esp_netif_destroy(modem_netif);
    modem_netif = NULL;

    return ESP_OK;
}

esp_err_t modem_send_sms(esp_modem_dce_t *dce, char *telno, char *msg) {
    esp_err_t err = esp_modem_send_sms(dce, telno, msg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_send_sms() failed with %d", err);
        return err;
    }
    ESP_LOGI(TAG, "SMS sent successfully");

    return ESP_OK;
}

esp_err_t modem_get_gps_raw(esp_modem_dce_t *dce, char *buf, size_t buf_len) {
    char recv_buf[CONFIG_ESP_MODEM_C_API_STR_MAX];
    esp_modem_at(dce, "AT+CGNSPWR=1\r", recv_buf, pdMS_TO_TICKS(1000));
    esp_modem_at(dce, "AT+CGNSINF\r", recv_buf, pdMS_TO_TICKS(1000));
    strncpy(buf, recv_buf, buf_len);
    esp_modem_at(dce, "AT+CGNSPWR=0\r", recv_buf, pdMS_TO_TICKS(1000));
    return ESP_OK;
}
