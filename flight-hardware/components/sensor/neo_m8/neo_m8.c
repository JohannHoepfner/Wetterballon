#include "neo_m8.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>

extern esp_err_t gps_init(void);
extern esp_err_t gps_restart(void);
extern bool gps_read(double *latitude, double *longitude, float *altitude, int *hour, int *minute, float *second,
                     bool *valid);

static const char *TAG = "sensor/neo_m8";
static unsigned int invalid_read_count;

static char gps_formatted[64];

Sensor neo_m8 = {
    .init = neo_m8_init,
    .read = neo_m8_read,
};

esp_err_t neo_m8_init(Sensor *self) {
    (void)self;

    esp_err_t err = gps_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NEO-M8 GPS sensor");
        return err;
    }

    ESP_LOGI(TAG, "NEO-M8 GPS sensor initialized");
    return ESP_OK;
}

char *neo_m8_read(Sensor *self) {
    (void)self;

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

    if (valid) {
        invalid_read_count = 0;
    } else if (++invalid_read_count >= 20) {
        ESP_LOGW(TAG, "Restarting NEO-M8 GPS after too many invalid reads");
        invalid_read_count = 0;
        if (gps_restart() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restart NEO-M8 GPS sensor");
        }
    }

    return gps_format(latitude, longitude, altitude, hour, minute, second, valid);
}

char *gps_format(double latitude, double longitude, double altitude, int hour, int minute, float second, bool valid) {
    if (valid) {
        snprintf(gps_formatted, sizeof(gps_formatted), "lat=%.6f,lon=%.6f,alt=%.2f,utc=%02d:%02d:%04.1f", latitude,
                 longitude, altitude, hour, minute, second);
    } else {
        snprintf(gps_formatted, sizeof(gps_formatted), "lat=%.6f,lon=%.6f,alt=%.0f,utc=%02d:%02d:%04.1f (invalid)",
                 latitude, longitude, altitude, hour, minute, second);
    }
    return gps_formatted;
}
