#include "include/databus.h"

#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"

_Static_assert(sizeof(struct databus_message) + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX) <= ESP_NOW_MAX_DATA_LEN,
               "sizeof(struct databus_message) + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)"
               "overflows ESP_NOW_MAX_LEN");

static const char *TAG = "DATABUS";

#define ESPNOW_MAXDELAY 512

static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

int set_time(time_t time) {
    struct timeval tv = {.tv_sec = time, .tv_usec = 0};
    return settimeofday(&tv, NULL);
}

esp_err_t databus_wifi_init(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_DATABUS_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    return ESP_OK;
}

#define DATABUS_MAX_NUM_CALLBACKS 32
void (*data_callbacks[DATABUS_MAX_NUM_CALLBACKS])(struct databus_message *);
void (*log_callbacks[DATABUS_MAX_NUM_CALLBACKS])(struct databus_message *);
int num_log_callbacks = 0;
int num_data_callbacks = 0;

esp_err_t databus_register_recv_callback(enum databus_message_type type, void (*callback)(struct databus_message *)) {
    switch (type) {
    case databus_message_type_data:
        data_callbacks[num_data_callbacks] = callback;
        num_data_callbacks++;
        break;
    case databus_message_type_log:
        if (num_log_callbacks >= DATABUS_MAX_NUM_CALLBACKS) {
            return ESP_ERR_INVALID_STATE;
        }
        log_callbacks[num_log_callbacks] = callback;
        num_log_callbacks++;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

void _databus_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    uint8_t *mac_addr = recv_info->src_addr;
    uint8_t *des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    if (!IS_BROADCAST_ADDR(des_addr)) {
        return;
    }

    char msg_buf[ESP_NOW_MAX_DATA_LEN];
    memcpy(msg_buf, data, len);
    struct databus_message msg;
    esp_err_t err = databus_parse_message(msg_buf, &msg);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Received invalid espnow message");
        return;
    }

    switch (msg.type) {
    case databus_message_type_timesync:
        ESP_LOGI(TAG, "Received timesync message for %lld", (unsigned long long)msg.time.time);
        set_time(msg.time.time);
        break;
    case databus_message_type_data:
        ESP_LOGI(TAG, "Received data message: \"%s\"", msg.data.message);
        for (int i = 0; i < num_data_callbacks; ++i) {
            if (data_callbacks[i] == NULL)
                continue;
            data_callbacks[i](&msg);
        }
        break;
    case databus_message_type_log:
        ESP_LOGI(TAG, "Received log message: \"%s\"", msg.log.message);
        for (int i = 0; i < num_log_callbacks; ++i) {
            if (log_callbacks[i] == NULL)
                continue;
            log_callbacks[i](&msg);
        }
        break;
    default:
        ESP_LOGE(TAG, "Received espnow message with invalid type");
        return;
    }

    ESP_LOGI(TAG, "%s", data);
}

esp_err_t databus_init() {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(_databus_espnow_recv_cb));
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        esp_now_deinit();
        return ESP_FAIL;
    }
    peer->channel = CONFIG_DATABUS_WIFI_CHANNEL;
    peer->ifidx = ESP_IF_WIFI_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);
    return ESP_OK;
}

esp_err_t databus_send(struct databus_message msg) {
    char msg_buf[sizeof(CONFIG_DATABUS_MESSAGE_PREFIX) + sizeof(struct databus_message)];
    memcpy(msg_buf, CONFIG_DATABUS_MESSAGE_PREFIX, sizeof(CONFIG_DATABUS_MESSAGE_PREFIX));
    memcpy(msg_buf + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX), &msg, sizeof(msg));
    return esp_now_send(broadcast_mac, (const uint8_t *)msg_buf, sizeof(msg_buf));
}

esp_err_t databus_send_timesync(time_t time) {
    struct databus_message msg = {.send_time = time, .type = databus_message_type_timesync, {.time = {time}}};
    return databus_send(msg);
}

esp_err_t databus_send_log(time_t time, char *msg_str) {
    struct databus_message msg = {.send_time = time, .type = databus_message_type_log};
    strncpy(msg.log.message, msg_str, sizeof(msg.log.message));
    return databus_send(msg);
}

esp_err_t databus_send_data(time_t time, char *msg_str) {
    struct databus_message msg = {.send_time = time, .type = databus_message_type_data};
    strncpy(msg.data.message, msg_str, sizeof(msg.data.message));
    return databus_send(msg);
}

esp_err_t databus_parse_message(char *msg_buf, struct databus_message *out_msg) {
    if (memcmp(msg_buf, CONFIG_DATABUS_MESSAGE_PREFIX, sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(out_msg, msg_buf + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX), sizeof(struct databus_message));

    switch (out_msg->type) {
    case databus_message_type_timesync:
        break;
    case databus_message_type_data:
        out_msg->data.message[sizeof(out_msg->data.message) - 1] = '\0';
        break;
    case databus_message_type_log:
        out_msg->log.message[sizeof(out_msg->log.message) - 1] = '\0';
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}
