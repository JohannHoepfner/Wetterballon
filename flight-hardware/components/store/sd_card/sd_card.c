#include "sd_card.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static const char *TAG = "store/sd_card";

Store sd_card = {
    .init = sdcard_init,
    .deinit = sdcard_deinit,
    .save = write_data,
    .load = read_databus_messages,
};

sdmmc_card_t *card;

esp_err_t sdcard_init(void) {
    esp_err_t err;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 8, .allocation_unit_size = 8192};
    const char mount_point[] = CONFIG_SD_CARD_MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_SD_CARD_PIN_MOSI,
        .miso_io_num = CONFIG_SD_CARD_PIN_MISO,
        .sclk_io_num = CONFIG_SD_CARD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    err = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus");
        return err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_SD_CARD_PIN_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (err != ESP_OK) {
        if (err == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the "
                          "CONFIG_SD_CARD_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG,
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(err));
        }
        return err;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

esp_err_t sdcard_deinit() {
    esp_vfs_fat_sdcard_unmount(CONFIG_SD_CARD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");
    return ESP_OK;
}

const char *data_file_path = CONFIG_SD_CARD_MOUNT_POINT "/data";
esp_err_t write_data(time_t time, char *msg_str) {
    FILE *data_file = fopen(data_file_path, "a");
    if (data_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_ERR_INVALID_STATE;
    }

    fprintf(data_file, "%llu:%u:%s\n", (unsigned long long)time,
            DATABUS_MSG_TYPE_DAT, msg_str);

    fclose(data_file);

    // ESP_LOGI(TAG, "Saved message to SD card: send_time=%llu, type=%u, message='%s'", (unsigned long long)time, DATABUS_MSG_TYPE_DAT, msg_str);
    return ESP_OK;
}

ssize_t read_databus_messages(struct databus_message *out_messages, size_t start, size_t count) {
    FILE *data_file = fopen(data_file_path, "r");
    if (data_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -1;
    }

    char line[sizeof(struct databus_message) + 32];
    size_t line_number = 0;
    size_t num = 0;
    while (num < count && fgets(line, sizeof(line), data_file) != NULL) {
        if (line_number++ < start) {
            continue;
        }

        unsigned long long send_time;
        unsigned int type;
        char payload[sizeof(out_messages[num].data.message)];
        if (sscanf(line, "%llu:%u:%219[^\n]", &send_time, &type, payload) != 3) {
            fclose(data_file);
            return -1;
        }

        memset(&out_messages[num], 0, sizeof(out_messages[num]));
        out_messages[num].send_time = send_time;
        out_messages[num].type = type;
        switch (type) {
        case DATABUS_MSG_TYPE_DAT:
            strncpy(out_messages[num].data.message, payload,
                    sizeof(out_messages[num].data.message) - 1);
            break;
        case DATABUS_MSG_TYPE_LOG:
            strncpy(out_messages[num].log.message, payload,
                    sizeof(out_messages[num].log.message) - 1);
            break;
        case DATABUS_MSG_TYPE_TMS:
            out_messages[num].timesync.time = strtoull(payload, NULL, 10);
            break;
        default:
            fclose(data_file);
            return -1;
        }
        ++num;
    }

    fclose(data_file);

    ESP_LOGI(TAG, "Read %zu messages from SD card starting at line %zu", num, start);
    return (ssize_t)num;
}
