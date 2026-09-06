#include "databus.h"
#include "databus_message.h"

#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"

static_assert(sizeof(struct databus_message) + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX) <= ESP_NOW_MAX_DATA_LEN,
              "sizeof(struct databus_message) + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)"
              "overflows ESP_NOW_MAX_LEN");

static const char *TAG = "intracom/databus";

uint8_t node_id = 0;
uint64_t mesage_id_counter = 0;

Intracom databus = {
    .init = databus_init, .send = databus_send_data, .register_recv_callback = databus_register_recv_callback};

#define ESPNOW_MAXDELAY 512

static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define IS_BROADCAST_ADDR(addr) (memcmp(addr, broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

int set_time(time_t time) {
    struct timeval tv = {.tv_sec = time, .tv_usec = 0};
    return settimeofday(&tv, NULL);
}

#define DATABUS_MAX_NUM_CALLBACKS 32
void (*data_callbacks[DATABUS_MAX_NUM_CALLBACKS])(struct databus_message *);
void (*log_callbacks[DATABUS_MAX_NUM_CALLBACKS])(struct databus_message *);
int num_log_callbacks = 0;
int num_data_callbacks = 0;

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

    if (memcmp(msg_buf, CONFIG_DATABUS_MESSAGE_PREFIX, sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)) != 0) {
        ESP_LOGE(TAG, "Received invalid espnow message wrong prefix");
        return;
    }

    struct databus_message msg;
    int err =
        databus_message_from_recv((struct databus_message *)(msg_buf + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)), &msg);

    if (err != 0) {
        ESP_LOGE(TAG, "Received invalid espnow message");
        return;
    }

    switch (msg.type) {
    case DATABUS_MSG_TYPE_DATA:
        ESP_LOGI(TAG, "Received data message '%.*s' from '%s' with id %llu", sizeof(msg.data.message), msg.data.message, databus_get_node_name(msg.node_id), (unsigned long long)msg.msg_id);
        for (int i = 0; i < num_data_callbacks; ++i) {
            if (data_callbacks[i] == NULL)
                continue;
            data_callbacks[i](&msg);
        }
        break;
    case DATABUS_MSG_TYPE_LOG:
        ESP_LOGI(TAG, "Received log message '%.*s' from '%s' with id %llu", sizeof(msg.log.message), msg.log.message, databus_get_node_name(msg.node_id), (unsigned long long)msg.msg_id);
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
}

esp_err_t databus_init(NODE_ID id) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_DATABUS_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

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

    node_id = id;
    ESP_LOGI(TAG, "Initializing databus with node ID: %llu", (unsigned long long)id);
    return ESP_OK;
}

esp_err_t databus_send(struct databus_message *msg) {
    char msg_buf[sizeof(CONFIG_DATABUS_MESSAGE_PREFIX) + sizeof(struct databus_message)];
    databus_message_to_send(msg, (struct databus_message *)(msg_buf + sizeof(CONFIG_DATABUS_MESSAGE_PREFIX)));
    memcpy(msg_buf, CONFIG_DATABUS_MESSAGE_PREFIX, sizeof(CONFIG_DATABUS_MESSAGE_PREFIX));
    return esp_now_send(broadcast_mac, (const uint8_t *)msg_buf, sizeof(msg_buf));
}

esp_err_t databus_send_data(time_t time, char *msg_str) {
    struct databus_message msg = {
        .send_time = time, .type = DATABUS_MSG_TYPE_DATA, .node_id = node_id, .msg_id = mesage_id_counter++};
    strncpy((char *)msg.data.message, msg_str, sizeof(msg.data.message));
    return databus_send(&msg);
}

esp_err_t databus_register_recv_callback(uint64_t message_type, void (*callback)(struct databus_message *)) {
    switch (message_type) {
    case DATABUS_MSG_TYPE_DATA:
        data_callbacks[num_data_callbacks] = callback;
        num_data_callbacks++;
        break;
    case DATABUS_MSG_TYPE_LOG:
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
