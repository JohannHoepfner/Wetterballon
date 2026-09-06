#include <memory>

#include "esp_log.h"
#include "gps.hpp"

namespace {
std::unique_ptr<espp::Gps> gps;
}

extern "C" esp_err_t gps_init(void) {
    if (gps != nullptr) {
        return ESP_OK;
    }

    gps = std::make_unique<espp::Gps>(espp::Gps::Config{
        .tx_io_num = GPIO_NUM_20,
        .rx_io_num = GPIO_NUM_21,
        .baud_rate = 9600,
        .auto_start = true,
    });

    return gps->is_running() ? ESP_OK : ESP_FAIL;
}

extern "C" esp_err_t gps_restart(void) {
    gps.reset();
    return gps_init();
}

extern "C" bool gps_read(double *latitude, double *longitude, float *altitude, int *hour, int *minute, float *second,
                         bool *valid) {
    if (gps == nullptr || latitude == nullptr || longitude == nullptr || altitude == nullptr || valid == nullptr ||
        hour == nullptr || minute == nullptr || second == nullptr) {
        return false;
    }

    const espp::GpsFix fix = gps->fix();

    *latitude = fix.latitude;
    *longitude = fix.longitude;
    *altitude = fix.altitude;
    *hour = fix.hour;
    *minute = fix.minute;
    *second = fix.second;
    *valid = fix.valid;

    // ESP_LOGI("sensor/neo_m8",
    //          "Fix: %.6f, %.6f alt %.1f m, %d sats, hdop %.1f, "
    //          "%02d:%02d:%04.1f UTC",
    //          fix.latitude, fix.longitude, fix.altitude, fix.num_satellites, fix.hdop, fix.hour, fix.minute,
    //          fix.second);

    return true;
}
