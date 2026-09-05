#include "sms.h"
#include "modem.h"

#include "esp_err.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "sms";

Intercom sms = {
    .init = sms_init,
    .deinit = sms_deinit,
    .send = sms_send_msg
};

esp_modem_dce_t *dce;

time_t now;
int tx_fail_counter = 0;

esp_err_t sms_init(void) {
    esp_err_t err = modem_start(&dce);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Modem init failed with %d %s", err, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

esp_err_t sms_deinit(void) {
    esp_err_t err = modem_stop(dce);
    return err;
}

esp_err_t sms_send_msg(char *msg, size_t msg_len) {
    if (tx_fail_counter > 5) {
        ESP_LOGE(TAG, "Too many failed transmissions, restarting modem");
        esp_err_t err = modem_reset(dce);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Modem restart failed with %d %s", err, esp_err_to_name(err));
            return err;
        }
        tx_fail_counter = 0;
    }

    int rssi, ber;

    // WAIT FOR SIGNAL
    esp_err_t err = get_signal_quality(dce, &rssi, &ber);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_signal_quality failed with %d %s", err,
                        esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);

    // PARSE DATA FOR SENDING
    time(&now);

    // SEND SMS
    err = modem_send_sms(dce, CONFIG_SIM_GATEWAY_NOTIFICATION_SMS_RECEIPIENT, "hello from wb");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "modem_send_sms() failed with %d", err);
        tx_fail_counter++;
        return err;
    }
    ESP_LOGI(TAG, "SMS sent successfully");

    return ESP_OK;
}
