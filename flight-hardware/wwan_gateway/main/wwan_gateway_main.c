#include "databus.h"
#include "databus_message.h"
#include "esp_err.h"
#include "modem.h"
#include "sd_card.h"
#include "sdkconfig.h"
#include "status_led.h"

#include <freertos/FreeRTOS.h>

#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_modem_api.h>
#include <esp_modem_c_api_types.h>
#include <esp_tls.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wwan_gateway";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
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

void espnow_data_callback(struct databus_message *msg) { save_databus_message(msg); }

esp_err_t send_data(char *data, size_t len) {
    char *local_response_buffer = calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
    esp_http_client_config_t http_client_config = {
        .host = "johann-hoepfner.de",
        .path = "/",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
        .disable_auto_redirect = true,
        .method = HTTP_METHOD_POST,
        .port = CONFIG_WWAN_GATEWAY_SERVER_HTTP_PORT,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .username = CONFIG_WWAN_GATEWAY_SERVER_HTTP_USERNAME,
        .password = CONFIG_WWAN_GATEWAY_SERVER_HTTP_PASSWORD,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);

    esp_http_client_set_post_field(http_client, data, len);
    esp_err_t http_err = esp_http_client_perform(http_client);
    if (http_err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(http_client),
                 esp_http_client_get_content_length(http_client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(http_err));
    }

    esp_http_client_cleanup(http_client);
    free(local_response_buffer);
    return ESP_OK;
}

void app_main(void) {
    int err;

    status_led_init();
    status_led_set(BLUE);

    ESP_ERROR_CHECK(sdcard_init());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    databus_wifi_init();
    databus_init();
    databus_register_recv_callback(DATABUS_MSG_TYPE_DAT, espnow_data_callback);

    esp_modem_dce_t *dce;
    esp_netif_t *esp_netif;
    err = modem_start(&dce, &esp_netif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Modem init failed with %d %s", err, esp_err_to_name(err));
        status_led_set(RED);
        return;
    }
    status_led_set(GREEN);

    size_t read_off = 0;

#define READ_CHUNK 64

    time_t last_sms_time = 0;

    for (int i = 0; true; i++) {

        // WAIT FOR SIGNAL

        int rssi, ber;
        err = esp_modem_get_signal_quality(dce, &rssi, &ber);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

        // PARSE DATA FOR SENDING

        time_t now;
        time(&now);
        ESP_LOGI(TAG, "time: %ld", (long int)now);

        if (now - last_sms_time > 120) {
            modem_send_sms(dce, CONFIG_WWAN_GATEWAY_NOTIFICATION_SMS_RECEIPIENT, "hello from wb");
            last_sms_time = now;
        }

        err = databus_send_timesync(now);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "databus_send_timesync(now) failed with %d %s", err, esp_err_to_name(err));
            status_led_set(RED);
            return;
        }

        struct databus_message *messages = malloc(sizeof(struct databus_message) * READ_CHUNK);
        int num = read_databus_messages(messages, read_off, READ_CHUNK);
        read_off += num;

        struct databus_message *buf = malloc(sizeof(struct databus_message) * num);

        for (int n = 0; n < num; ++n) {
            databus_message_to_send(messages + n, buf + n);
        }

        send_data((char *)buf, sizeof(struct databus_message) * num);

        free(messages);
        free(buf);

        status_led_set(color_mix(BLUE, color_mix(RED, BLUE, 0.5), (i % 100) / 100.));
    }

    status_led_set(BLUE);

    modem_stop(dce, esp_netif);
    ESP_ERROR_CHECK(sdcard_deinit());
}
