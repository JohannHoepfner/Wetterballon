#pragma once

#include "../../store.h"

#include "sd_protocol_types.h"
#include <driver/gpio.h>

struct store_sd_card_cfg {
    gpio_num_t pin_mosi;
    gpio_num_t pin_miso;
    gpio_num_t pin_clk;
    gpio_num_t pin_cs;
};

struct store_sd_card {
    struct store base;
    struct store_sd_card_cfg cfg;
    sdmmc_card_t *card;
};

void sd_card_store_create(struct store_sd_card *out, const struct store_sd_card_cfg *config);
