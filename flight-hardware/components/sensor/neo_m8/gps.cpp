#include <memory>

#include "esp_log.h"
#include "gps.hpp"

namespace {
std::unique_ptr<espp::Gps> gps;
}


static constexpr uint8_t kCfgNav5Air1g[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, // hdr, CFG-NAV5, len 36
    0x01, 0x00,                         // mask: apply dynModel only
    0x06,                               // dynModel: airborne <1g
    0x00,                               // fixMode (ignored by mask)
    0x00, 0x00, 0x00, 0x00,             // fixedAlt
    0x00, 0x00, 0x00, 0x00,             // fixedAltVar
    0x00, 0x00,                         // minElev, drLimit
    0x00, 0x00, 0x00, 0x00,             // pDop, tDop
    0x00, 0x00, 0x00, 0x00,             // pAcc, tAcc
    0x00, 0x00, 0x00, 0x00,             // staticHold, dgnss, cnoThresh*2
    0x00, 0x00,                         // reserved1
    0x00, 0x00,                         // staticHoldMaxDist
    0x00,                               // utcStandard
    0x00, 0x00, 0x00, 0x00, 0x00,       // reserved2
    0x55, 0xB4,                         // CK_A, CK_B
};

static void reassert_cfg(void *) {
    std::error_code ec;
    gps->write({reinterpret_cast<const char *>(kCfgNav5Air1g),
                sizeof(kCfgNav5Air1g)}, ec);
}

static esp_timer_handle_t s_cfg_timer;

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

    const esp_timer_create_args_t args = {.callback = reassert_cfg, .name = "gps_cfg"};
    esp_timer_create(&args, &s_cfg_timer);
    esp_timer_start_periodic(s_cfg_timer, 60ULL * 1000 * 1000);

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
