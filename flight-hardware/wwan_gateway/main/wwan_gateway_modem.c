#include "wwan_gateway_modem.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_modem_c_api_types.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_netif_sntp.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/projdefs.h"
#include "sdkconfig.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const int CONNECT_BIT = BIT0;
static const int DISCONNECT_BIT = BIT1;
static const int GOT_DATA_BIT = BIT2;

static const char *TAG = "wwan_gateway";

#if defined(CONFIG_WWAN_GATEWAY_FLOW_CONTROL_NONE)
#define WWAN_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_WWAN_GATEWAY_FLOW_CONTROL_SW)
#define WWAN_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_WWAN_GATEWAY_FLOW_CONTROL_HW)
#define WWAN_GATEWAY_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif

static EventGroupHandle_t event_group = NULL;

static void _on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "PPP state changed event %" PRIu32, event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t **p_netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", *p_netif);
    }
}

static void _on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "IP event! %" PRIu32, event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        xEventGroupSetBits(event_group, DISCONNECT_BIT);
    } else if (event_id == IP_EVENT_GOT_IP6) {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer; // Buffer to store response of http request from
                                // event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data) {
            // we are just starting to copy the output data into the use
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        if (!esp_http_client_is_chunked_response(evt->client)) {
            int copy_len = 0;
            if (evt->user_data) {
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len) {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            } else {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL) {
                    output_buffer = (char *)calloc(content_len + 1, sizeof(char));
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (content_len - output_len));
                if (copy_len) {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

esp_err_t modem_start(esp_modem_dce_t **dce_out, esp_netif_t **esp_netif_out) {
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &_on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_on_ppp_changed, NULL));

    esp_err_t err;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_WWAN_GATEWAY_MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    event_group = xEventGroupCreate();

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CONFIG_WWAN_GATEWAY_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_WWAN_GATEWAY_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_WWAN_GATEWAY_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_WWAN_GATEWAY_MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = WWAN_GATEWAY_FLOW_CONTROL;
    dte_config.uart_config.rx_buffer_size = CONFIG_WWAN_GATEWAY_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_WWAN_GATEWAY_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_WWAN_GATEWAY_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_WWAN_GATEWAY_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_WWAN_GATEWAY_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_WWAN_GATEWAY_MODEM_UART_RX_BUFFER_SIZE / 2;

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);

    assert(dce);
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
        gpio_set_direction(CONFIG_WWAN_GATEWAY_MODEM_PWK_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_WWAN_GATEWAY_MODEM_PWK_PIN, 0);
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        gpio_set_level(CONFIG_WWAN_GATEWAY_MODEM_PWK_PIN, 1);
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

    xEventGroupClearBits(event_group, CONNECT_BIT | GOT_DATA_BIT | DISCONNECT_BIT);

    int rssi, ber;
    err = esp_modem_get_signal_quality(dce, &rssi, &ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

    err = esp_modem_set_mode(dce, ESP_MODEM_MODE_CMUX);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_CMUX) failed with %d", err);
        return err;
    }

    ESP_LOGI(TAG, "Waiting for IP address");
    xEventGroupWaitBits(event_group, CONNECT_BIT | DISCONNECT_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(60000));

    if ((xEventGroupGetBits(event_group) & CONNECT_BIT) != CONNECT_BIT) {
        ESP_LOGW(TAG, "Modem not connected, switching back to the command mode");
        err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
            return err;
        }
        ESP_LOGI(TAG, "Command mode restored");
        return err;
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(5000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_sntp_sync_wait() failed with %d", err);
    }

    *dce_out = dce;
    *esp_netif_out = esp_netif;

    return ESP_OK;
}

esp_err_t modem_stop(esp_modem_dce_t *dce, esp_netif_t *esp_netif) {
    // UART DTE clean-up
    esp_modem_destroy(dce);
    esp_netif_destroy(esp_netif);

    return ESP_OK;
}

esp_err_t modem_send_sms(esp_modem_dce_t *dce, char *telno, char *msg) {
    int err;
    err = esp_modem_sms_txt_mode(dce, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Setting text mode failed");
        return err;
    }
    err = esp_modem_sms_character_set(dce);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Setting GSM character set failed");
        return err;
    }

    err = esp_modem_send_sms(dce, telno, msg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_send_sms() failed with %d", err);
        return err;
    }

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
