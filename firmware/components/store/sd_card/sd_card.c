#include "sd_card.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static const char *TAG = "store/sd_card";

static esp_err_t
sd_card_init(struct store *base) {
    struct store_sd_card *self = __containerof(base, struct store_sd_card, base);

    esp_err_t init_err;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 8, .allocation_unit_size = 8192};
    const char mount_point[] = CONFIG_SD_CARD_MOUNT_POINT;
    ESP_LOGI(TAG, "STATUS_INDICATOR_INITIALIZING SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = self->cfg.pin_mosi,
        .miso_io_num = self->cfg.pin_miso,
        .sclk_io_num = self->cfg.pin_clk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    init_err = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus");
        return init_err;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = self->cfg.pin_cs;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    init_err = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &self->card);

    if (init_err != ESP_OK) {
        if (init_err == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the "
                          "CONFIG_SD_CARD_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG,
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(init_err));
        }
        return init_err;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}

static esp_err_t
sd_card_deinit(struct store *base) {
    struct store_sd_card *self = __containerof(base, struct store_sd_card, base);

    sdmmc_card_t *card = self->card;
    self->card = NULL;
    esp_vfs_fat_sdcard_unmount(CONFIG_SD_CARD_MOUNT_POINT, card);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    esp_err_t err = spi_bus_free(host.slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to free SPI bus: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Card unmounted");
    return ESP_OK;
}

static esp_err_t
sd_card_reinit(struct store *base) {
    sd_card_deinit(base);
    return sd_card_init(base);
}

const char *data_file_path = CONFIG_SD_CARD_MOUNT_POINT "/data";
static esp_err_t
sd_card_write_data(struct store *, time_t time, const char *msg_str) {
    FILE *data_file = fopen(data_file_path, "a");
    if (data_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_ERR_INVALID_STATE;
    }

    if (fprintf(data_file, "%llu:%s\n", (unsigned long long)time, msg_str) < 0) {
        ESP_LOGE(TAG, "Failed to write data to file");
        fclose(data_file);
        return ESP_FAIL;
    }

    if (fclose(data_file) != 0) {
        ESP_LOGE(TAG, "Failed to close data file after writing");
        return ESP_FAIL;
    }

    // ESP_LOGI(TAG, "Saved message to SD card: send_time=%llu, type=%u, message='%s'", (unsigned long long)time,
    // DATABUS_MSG_TYPE_DATA, msg_str);
    return ESP_OK;
}

static esp_err_t
sd_card_read_lines(struct store *, size_t max_lines, char *buffer, size_t buffer_size, size_t *lines_read) {
    size_t previous_lines_read = *lines_read;
    FILE *data_file = fopen(data_file_path, "r");
    if (data_file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_ERR_INVALID_STATE;
    }

    if (fseek(data_file, 0, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to seek to the beginning of the data file");
        fclose(data_file);
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t skipped_lines = 0; skipped_lines < previous_lines_read; skipped_lines++) {
        int character;
        do {
            character = fgetc(data_file);
        } while (character != '\n' && character != EOF);

        if (character == EOF) {
            break;
        }
    }

    size_t total_bytes_read = 0;

    while (*lines_read < previous_lines_read + max_lines && !feof(data_file)) {
        if (fgets(buffer + total_bytes_read, buffer_size - total_bytes_read, data_file) == NULL) {
            break; // EOF or error
        }
        total_bytes_read += strlen(buffer + total_bytes_read);
        (*lines_read)++;
    }

    fclose(data_file);

    ESP_LOGI(TAG, "Read until line %zu from SD card, total bytes read: %zu", *lines_read, total_bytes_read);

    return ESP_OK;
}

void
sd_card_store_create(struct store_sd_card *out, const struct store_sd_card_cfg *config) {
    *out = (struct store_sd_card){
        .base =
            {
                   .init = sd_card_init,
                   .reinit = sd_card_reinit,
                   .deinit = sd_card_deinit,
                   .save = sd_card_write_data,
                   .read_lines = sd_card_read_lines,
                   },
        .cfg = *config,
    };
}
