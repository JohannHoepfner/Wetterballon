#include <memory>

#include "gps.hpp"

namespace {
std::unique_ptr<espp::Gps> gps;
}

extern "C" esp_err_t gps_init(void) {
    if (gps != nullptr) {
        return ESP_OK;
    }

    gps = std::make_unique<espp::Gps>(espp::Gps::Config{
        .uart_port = UART_NUM_1,
        .tx_io_num = GPIO_NUM_20,
        .rx_io_num = GPIO_NUM_21,
        .baud_rate = 9600,
    });

    return gps->is_running() ? ESP_OK : ESP_FAIL;
}

extern "C" bool gps_read(double *latitude, double *longitude, float *altitude, bool *valid) {
    if (gps == nullptr || latitude == nullptr || longitude == nullptr || altitude == nullptr || valid == nullptr) {
        return false;
    }

    const espp::GpsFix fix = gps->fix();
    *latitude = fix.latitude;
    *longitude = fix.longitude;
    *altitude = fix.altitude;
    *valid = fix.valid;
    return true;
}
