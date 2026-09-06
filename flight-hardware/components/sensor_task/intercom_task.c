
static void send_task(void *arg)
{
    (void)arg;

    int backoff_sec = 5;
    while (sim_modem.init() != ESP_OK) {
        ESP_LOGE(TAG, "sim_modem.init() failed, retrying in %d s", backoff_sec);
        vTaskDelay(pdMS_TO_TICKS(backoff_sec * 1000));
        backoff_sec = backoff_sec < 300 ? backoff_sec * 2 : 300;
    }
    ESP_LOGI(TAG, "Modem ready");

    static char body[AGG_BUF_SIZE];
    while (1) {
        size_t snapshot_len;

        xSemaphoreTake(s_agg_mutex, portMAX_DELAY);
        snapshot_len = s_agg_len;
        memcpy(body, s_agg_buf, snapshot_len);
        xSemaphoreGive(s_agg_mutex);

        size_t body_len = snapshot_len;
        if (snapshot_len == 0) {
            body_len = snprintf(body, sizeof(body), "(no intracom messages since last send)\r\n");
        }

        esp_err_t err = sim_modem.send(body, body_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Sending email failed, will retry next cycle (messages stay queued)");
        } else if (snapshot_len > 0) {
            xSemaphoreTake(s_agg_mutex, portMAX_DELAY);
            if (s_agg_len >= snapshot_len) {
                size_t remaining = s_agg_len - snapshot_len;
                memmove(s_agg_buf, s_agg_buf + snapshot_len, remaining);
                s_agg_len = remaining;
            }
            xSemaphoreGive(s_agg_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_MODEM_SEND_INTERVAL_SEC * 1000));
    }
}
