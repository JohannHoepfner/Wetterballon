#include "neo_m8.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <sys/time.h>

extern esp_err_t gps_init(void);
extern esp_err_t gps_restart(void);
extern bool gps_read(double *latitude, double *longitude, float *altitude, int *hour, int *minute, float *second,
                     bool *valid);

static const char *TAG = "sensor/neo_m8";

static char gps_formatted[64];

#define NEO_M8_MAX_CALLBACKS 32

static sensor_callback_t callbacks[NEO_M8_MAX_CALLBACKS];
static size_t callback_count;

static esp_err_t
neo_m8_on_receive(sensor_callback_t callback) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (callback_count >= NEO_M8_MAX_CALLBACKS) {
        return ESP_ERR_NO_MEM;
    }

    callbacks[callback_count++] = callback;

    return ESP_OK;
}

static esp_err_t
neo_m8_init(void) {
    esp_err_t err = gps_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NEO-M8 GPS sensor");
        return err;
    }

    ESP_LOGI(TAG, "NEO-M8 GPS sensor initialized");
    return ESP_OK;
}

static char *
gps_format(double latitude, double longitude, double altitude, int hour, int minute, float second, bool valid) {
    if (valid) {
        snprintf(gps_formatted, sizeof(gps_formatted), "lat=%.6f,lon=%.6f,alt=%.2f,utc=%02d:%02d:%04.1f", latitude,
                 longitude, altitude, hour, minute, second);
    } else {
        snprintf(gps_formatted, sizeof(gps_formatted), "lat=%.6f,lon=%.6f,alt=%.0f,utc=%02d:%02d:%04.1f (invalid)",
                 latitude, longitude, altitude, hour, minute, second);
    }
    return gps_formatted;
}

static char *
neo_m8_read(void) {
    double latitude;
    double longitude;
    float altitude;
    int hour, minute;
    float second;
    bool valid;

    if (!gps_read(&latitude, &longitude, &altitude, &hour, &minute, &second, &valid)) {
        ESP_LOGE(TAG, "Failed to read NEO-M8 GPS sensor");
        return "gps_error";
    }

    char *formatted_gps = gps_format(latitude, longitude, altitude, hour, minute, second, valid);

    for (size_t i = 0; i < callback_count; i++) {
        callbacks[i](formatted_gps);
    }

    return formatted_gps;
}

struct sensor neo_m8 = {
    .init = neo_m8_init,
    .read = neo_m8_read,
    .on_receive = neo_m8_on_receive,
};
