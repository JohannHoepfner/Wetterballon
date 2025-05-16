#include "databus.h"
#include "databus_message.h"
#include "esp_err.h"
#include "sd_card.h"
#include "sdkconfig.h"
#include "stdlib.h"
#include "wwan_gateway_modem.h"
#include "status_led.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_modem_c_api_types.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "wwan_gateway";

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

    ESP_LOGI(TAG, "Starting %d", sizeof(struct databus_message));

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
    databus_register_recv_callback(databus_message_type_data, espnow_data_callback);

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

        char *buf = malloc(sizeof(struct databus_message) * num);

        for (int n = 0; n < num; ++n) {
            databus_message_to_bytes(messages + n, buf + n * sizeof(struct databus_message));
        }

        send_data(buf, sizeof(struct databus_message) * num);

        free(messages);
        free(buf);

        status_led_set(color_mix(BLUE, color_mix(RED, BLUE, 0.5), (i % 100) / 100.));
    }

    status_led_set(BLUE);

    modem_stop(dce, esp_netif);
    ESP_ERROR_CHECK(sdcard_deinit());
}
