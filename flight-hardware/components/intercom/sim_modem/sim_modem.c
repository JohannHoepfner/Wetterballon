#include "sim_modem.h"

#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "sdkconfig.h"

static const char *TAG = "intercom/sim_modem";

Intercom sim_modem = {
    .init = sim_modem_init,
    .deinit = sim_modem_deinit,
    .send = sim_modem_send_msg,
};

static EventGroupHandle_t s_event_group;
#define CONNECT_BIT BIT0

static volatile bool s_ppp_connected = false;
static bool s_events_registered = false;
static esp_modem_dce_t *s_dce;
static esp_netif_t *s_netif;

static void on_ppp_changed(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "PPP state changed event %" PRId32, event_id);
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "PPP up, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_ppp_connected = true;
        xEventGroupSetBits(s_event_group, CONNECT_BIT);
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGW(TAG, "PPP lost IP");
        s_ppp_connected = false;
    }
}

/* Reads one (possibly multi-line) SMTP reply and checks it starts with `expect_code`. */
static esp_err_t smtp_expect(int sock, int expect_code) {
    char buf[512];
    int have_final_line = 0;
    int last_code = 0;

    while (!have_final_line) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            ESP_LOGE(TAG, "SMTP recv failed (%d)", n);
            return ESP_FAIL;
        }
        buf[n] = '\0';
        ESP_LOGD(TAG, "S: %s", buf);

        char *line = buf;
        while (line && *line) {
            if (strlen(line) < 4) {
                break;
            }
            last_code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
            /* "nnn " ends a (possibly multi-line) reply, "nnn-" continues it */
            have_final_line = (line[3] == ' ');
            char *next = strstr(line, "\r\n");
            line = next ? next + 2 : NULL;
        }
    }

    if (last_code != expect_code) {
        ESP_LOGE(TAG, "SMTP expected %d, got %d", expect_code, last_code);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t smtp_send(int sock, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "C: %s", buf);
    if (send(sock, buf, len, 0) != len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* Like smtp_send() but for arbitrary-length data (the aggregated message body can be
 * bigger than smtp_send()'s fixed 512-byte formatting buffer) - writes it as-is,
 * looping over partial send()s. */
static esp_err_t smtp_send_raw(int sock, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, data + sent, len - sent, 0);
        if (n <= 0) {
            ESP_LOGE(TAG, "SMTP send failed: errno %d", errno);
            return ESP_FAIL;
        }
        sent += (size_t)n;
    }
    return ESP_OK;
}

static esp_err_t send_email_once(const char *body, size_t body_len) {
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", CONFIG_MODEM_SMTP_PORT);

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int gai_err = getaddrinfo(CONFIG_MODEM_SMTP_HOST, port_str, &hints, &res);
    if (gai_err != 0 || res == NULL) {
        ESP_LOGE(TAG, "getaddrinfo(%s) failed: %d", CONFIG_MODEM_SMTP_HOST, gai_err);
        return ESP_FAIL;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno %d", errno);
        freeaddrinfo(res);
        return ESP_FAIL;
    }

    struct timeval tv = {.tv_sec = 20};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    esp_err_t err = ESP_FAIL;
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "connect() to %s:%d failed: errno %d", CONFIG_MODEM_SMTP_HOST, CONFIG_MODEM_SMTP_PORT, errno);
        goto out;
    }
    freeaddrinfo(res);
    res = NULL;

    ESP_LOGI(TAG, "Connected to %s:%d, starting SMTP dialog", CONFIG_MODEM_SMTP_HOST, CONFIG_MODEM_SMTP_PORT);

    if (smtp_expect(sock, 220) != ESP_OK) {
        goto out;
    }
    if (smtp_send(sock, "HELO esp32-modem-poc\r\n") != ESP_OK || smtp_expect(sock, 250) != ESP_OK) {
        goto out;
    }
    if (smtp_send(sock, "MAIL FROM:<%s>\r\n", CONFIG_MODEM_MAIL_FROM) != ESP_OK || smtp_expect(sock, 250) != ESP_OK) {
        goto out;
    }
    if (smtp_send(sock, "RCPT TO:<%s>\r\n", CONFIG_MODEM_MAIL_TO) != ESP_OK || smtp_expect(sock, 250) != ESP_OK) {
        goto out;
    }
    if (smtp_send(sock, "DATA\r\n") != ESP_OK || smtp_expect(sock, 354) != ESP_OK) {
        goto out;
    }

    if (smtp_send(sock,
                  "From: %s\r\n"
                  "To: %s\r\n"
                  "Subject: Modem POC ping\r\n"
                  "\r\n",
                  CONFIG_MODEM_MAIL_FROM, CONFIG_MODEM_MAIL_TO) != ESP_OK) {
        goto out;
    }
    if (smtp_send_raw(sock, body, body_len) != ESP_OK) {
        goto out;
    }
    if (smtp_send(sock, "\r\n.\r\n") != ESP_OK) {
        goto out;
    }
    if (smtp_expect(sock, 250) != ESP_OK) {
        goto out;
    }

    smtp_send(sock, "QUIT\r\n");
    smtp_expect(sock, 221);

    ESP_LOGI(TAG, "Email accepted by %s", CONFIG_MODEM_SMTP_HOST);
    err = ESP_OK;

out:
    if (res) {
        freeaddrinfo(res);
    }
    close(sock);
    return err;
}

/* Redials if the link dropped, bounded by a timeout - never blocks forever if the
 * cellular network is down. */
static esp_err_t ensure_connected(void) {
    if (s_ppp_connected) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "PPP link down, waiting for it to (re)connect...");
    EventBits_t bits = xEventGroupWaitBits(s_event_group, CONNECT_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(60000));
    if (bits & CONNECT_BIT) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "No PPP link yet, redialing...");
    esp_modem_set_mode(s_dce, ESP_MODEM_MODE_COMMAND);
    esp_modem_set_mode(s_dce, ESP_MODEM_MODE_DATA);
    return ESP_FAIL;
}

/* modem_init() can be called repeatedly by a caller retrying after a failure (e.g. the
 * cellular network being down at boot), so anything that isn't safe to redo - event
 * loop/handler registration, the event group - is only ever done once; anything
 * per-attempt (netif, dce) is torn down via sim_modem_deinit() before returning on failure
 * so a retry starts from a clean slate instead of leaking a netif/dce each time. */
esp_err_t sim_modem_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (!s_events_registered) {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
        s_event_group = xEventGroupCreate();
        s_events_registered = true;
    } else {
        xEventGroupClearBits(s_event_group, CONNECT_BIT);
    }
    s_ppp_connected = false;

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    s_netif = esp_netif_new(&netif_ppp_config);
    if (s_netif == NULL) {
        return ESP_FAIL;
    }

    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.uart_config.tx_io_num = CONFIG_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_MODEM_UART_CTS_PIN;
    dte_config.uart_config.flow_control = ESP_MODEM_FLOW_CONTROL_NONE;

    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    s_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, s_netif);
    if (s_dce == NULL) {
        sim_modem_deinit();
        return ESP_FAIL;
    }

#if CONFIG_MODEM_NEED_SIM_PIN
    bool pin_ok = false;
    if (esp_modem_read_pin(s_dce, &pin_ok) == ESP_OK && !pin_ok) {
        if (esp_modem_set_pin(s_dce, CONFIG_MODEM_SIM_PIN) != ESP_OK) {
            sim_modem_deinit();
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    int rssi, ber;
    if (esp_modem_get_signal_quality(s_dce, &rssi, &ber) == ESP_OK) {
        ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    }

    ESP_LOGI(TAG, "Switching modem to data mode...");
    if (esp_modem_set_mode(s_dce, ESP_MODEM_MODE_DATA) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to switch modem to data mode (possibly mode already set), continuing");
    }

    return ESP_OK;
}

esp_err_t sim_modem_deinit(void) {
    if (s_dce) {
        esp_modem_destroy(s_dce);
        s_dce = NULL;
    }
    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    s_ppp_connected = false;
    return ESP_OK;
}

esp_err_t sim_modem_send_msg(char *buf, size_t buflen) {
    if (ensure_connected() != ESP_OK) {
        return ESP_FAIL;
    }
    return send_email_once(buf, buflen);
}
