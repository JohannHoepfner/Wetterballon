#include "neo_m8.h"

#include <esp_log.h>
#include <esp_err.h>
#include <stdio.h>
#include <stdbool.h>

extern esp_err_t gps_init(void);
extern bool gps_read(double *latitude, double *longitude, float *altitude, bool *valid);

static const char *TAG = "sensor/neo_m8";

static char gps_formatted[64];

Sensor neo_m8 = {
    .init = neo_m8_init,
    .read = neo_m8_read,
};

esp_err_t neo_m8_init(Sensor *self)
{
    (void)self;

    esp_err_t err = gps_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NEO-M8 GPS sensor");
        return err;
    }

    ESP_LOGI(TAG, "NEO-M8 GPS sensor initialized");
    return ESP_OK;
}

char *neo_m8_read(Sensor *self)
{
    (void)self;

    double latitude;
    double longitude;
    float altitude;
    bool valid;
    if (!gps_read(&latitude, &longitude, &altitude, &valid)) {
        ESP_LOGE(TAG, "Failed to read NEO-M8 GPS sensor");
        return "gps_error";
    }

    if (!valid) {
        return "gps_no_fix";
    }

    return gps_format(latitude, longitude, altitude);
}

char *gps_format(double latitude, double longitude, double altitude)
{
    snprintf(gps_formatted, sizeof(gps_formatted), "la=%.6f, lo=%.6f, al=%.2f", latitude, longitude, altitude);
    return gps_formatted;
}
