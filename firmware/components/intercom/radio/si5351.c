/*
 * Copyright 2024 Emiliano Gonzalez (egonzalez . hiperion @ gmail . com))
 * * Project Site: https://github.com/hiperiondev/esp32-si5351 *
 *
 * This is based on other projects:
 *    Si5351 library for Arduino: Jason Milldrum <milldrum@gmail.com>, Dana H. Myers <k6jq@comcast.net>
 *    HAL-based Si5351 driver for STM32: https://github.com/afiskon/stm32-si5351
 *    Arduino Si5351 library tuned for size and click free: https://github.com/pavelmc/Si5351mcu
 *    Others (see individual files)
 *
 *    please contact their authors for more information.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <esp_err.h>
#include <esp_log.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2c_master.h"

#include "si5351.h"

static const char *TAG = "si5351";

/////////////////////////////// I2C functions ///////////////////////////////

int8_t
si5351_i2c_init(si5351_t *si5351_dev, i2c_port_num_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio,
                uint32_t clk_speed) {
    si5351_dev->i2c_dev.i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    si5351_dev->i2c_dev.i2c_bus_config.i2c_port = I2C_NUM_0;
    si5351_dev->i2c_dev.i2c_bus_config.scl_io_num = scl_gpio;
    si5351_dev->i2c_dev.i2c_bus_config.sda_io_num = sda_gpio;
    si5351_dev->i2c_dev.i2c_bus_config.glitch_ignore_cnt = 7;
    si5351_dev->i2c_dev.i2c_bus_config.flags.enable_internal_pullup = true;
    // si5351_dev->i2c_dev.i2c_bus_config.intr_priority = 0;
    // si5351_dev->i2c_dev.i2c_bus_config.trans_queue_depth = 128;

    si5351_dev->i2c_dev.i2c_dev_conf.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    si5351_dev->i2c_dev.i2c_dev_conf.device_address = SI5351_BUS_BASE_ADDR;
    si5351_dev->i2c_dev.i2c_dev_conf.scl_speed_hz = clk_speed;

    if (i2c_new_master_bus(&(si5351_dev->i2c_dev.i2c_bus_config), &(si5351_dev->i2c_dev.i2c_bus_handle)) != ESP_OK) {
        ESP_LOGI(TAG, "i2c new master bus FAIL");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "i2c new master bus OK");

    if (i2c_master_probe(si5351_dev->i2c_dev.i2c_bus_handle, SI5351_BUS_BASE_ADDR, SI5351_I2C_TIMEOUT_MS) != ESP_OK) {
        ESP_LOGI(TAG, "device not found");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "device found at address: 0x%02x", SI5351_BUS_BASE_ADDR);

    return ESP_OK;
}

static int8_t
si5351_i2c_read(si5351_t *si5351_dev, uint8_t addr) {
    uint8_t val = 0;

    if (i2c_master_bus_add_device(si5351_dev->i2c_dev.i2c_bus_handle, &(si5351_dev->i2c_dev.i2c_dev_conf),
                                  &(si5351_dev->i2c_dev.i2c_dev_handle)) != ESP_OK) {
        return 0;
    }

    if (i2c_master_transmit_receive(si5351_dev->i2c_dev.i2c_dev_handle, &addr, 1, &val, 1, SI5351_I2C_TIMEOUT_MS) !=
        ESP_OK) {
        return 0;
    }

    if (i2c_master_bus_rm_device(si5351_dev->i2c_dev.i2c_dev_handle) != ESP_OK) {
        return 0;
    }

    return val;
}

static uint8_t
si5351_i2c_write(si5351_t *si5351_dev, uint8_t addr, uint8_t data) {
    uint8_t dataw[2];

    dataw[0] = addr;
    dataw[1] = data;

    if (i2c_master_bus_add_device(si5351_dev->i2c_dev.i2c_bus_handle, &(si5351_dev->i2c_dev.i2c_dev_conf),
                                  &(si5351_dev->i2c_dev.i2c_dev_handle)) != ESP_OK) {
        return ESP_FAIL;
    }

    if (i2c_master_transmit(si5351_dev->i2c_dev.i2c_dev_handle, dataw, 2, SI5351_I2C_TIMEOUT_MS) != ESP_OK) {
        return ESP_FAIL;
    }

    if (i2c_master_bus_rm_device(si5351_dev->i2c_dev.i2c_dev_handle) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static inline uint8_t
si5351_i2c_write_bulk(si5351_t *si5351_dev, uint8_t addr, uint8_t bytes, uint8_t *data) {
    uint8_t dataw[bytes + 1];

    dataw[0] = addr;
    memcpy(dataw + 1, data, bytes);

    if (i2c_master_bus_add_device(si5351_dev->i2c_dev.i2c_bus_handle, &(si5351_dev->i2c_dev.i2c_dev_conf),
                                  &(si5351_dev->i2c_dev.i2c_dev_handle)) != ESP_OK) {
        return ESP_FAIL;
    }

    uint8_t ret = i2c_master_transmit(si5351_dev->i2c_dev.i2c_dev_handle, dataw, bytes + 1, SI5351_I2C_TIMEOUT_MS);

    if (i2c_master_bus_rm_device(si5351_dev->i2c_dev.i2c_dev_handle) != ESP_OK) {
        return ESP_FAIL;
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

static uint64_t
si5351_pll_calc(si5351_t *si5351_dev, enum si5351_pll pll, uint64_t freq, struct si5351_reg_set *reg,
                int32_t correction, uint8_t vcxo) {
    uint64_t ref_freq;
    if (pll == SI5351_PLLA) {
        ref_freq = si5351_dev->si5351_xtal_freq[(uint8_t)si5351_dev->plla_ref_osc] * SI5351_FREQ_MULT;
    } else {
        ref_freq = si5351_dev->si5351_xtal_freq[(uint8_t)si5351_dev->pllb_ref_osc] * SI5351_FREQ_MULT;
    }
    // ref_freq = 15974400ULL * SI5351_FREQ_MULT;
    uint32_t a, b, c, p1, p2, p3;
    uint64_t lltmp; //, denom;

    // Factor calibration value into nominal crystal frequency
    // Measured in parts-per-billion
    ref_freq = ref_freq + (int32_t)((((((int64_t)correction) << 31) / 1000000000LL) * ref_freq) >> 31);

    // PLL bounds checking
    if (freq < SI5351_PLL_VCO_MIN * SI5351_FREQ_MULT) {
        freq = SI5351_PLL_VCO_MIN * SI5351_FREQ_MULT;
    }
    if (freq > SI5351_PLL_VCO_MAX * SI5351_FREQ_MULT) {
        freq = SI5351_PLL_VCO_MAX * SI5351_FREQ_MULT;
    }

    // Determine integer part of feedback equation
    a = freq / ref_freq;

    if (a < SI5351_PLL_A_MIN) {
        freq = ref_freq * SI5351_PLL_A_MIN;
    }
    if (a > SI5351_PLL_A_MAX) {
        freq = ref_freq * SI5351_PLL_A_MAX;
    }

    // Find best approximation for b/c = fVCO mod fIN
    // denom = 1000ULL * 1000ULL;
    // lltmp = freq % ref_freq;
    // lltmp *= denom;
    // do_div(lltmp, ref_freq);

    // b = (((uint64_t)(freq % ref_freq)) * RFRAC_DENOM) / ref_freq;
    if (vcxo) {
        b = (((uint64_t)(freq % ref_freq)) * 1000000ULL) / ref_freq;
        c = 1000000ULL;
    } else {
        b = (((uint64_t)(freq % ref_freq)) * RFRAC_DENOM) / ref_freq;
        c = b ? RFRAC_DENOM : 1;
    }

    // Calculate parameters
    p1 = 128 * a + ((128 * b) / c) - 512;
    p2 = 128 * b - c * ((128 * b) / c);
    p3 = c;

    // Recalculate frequency as fIN * (a + b/c)
    lltmp = ref_freq;
    lltmp *= b;
    do_div(lltmp, c);
    freq = lltmp;
    freq += ref_freq * a;

    reg->p1 = p1;
    reg->p2 = p2;
    reg->p3 = p3;

    if (vcxo) {
        return (uint64_t)(128 * a * 1000000ULL + b);
    } else {
        return freq;
    }
}

static uint64_t
si5351_multisynth_calc(uint64_t freq, uint64_t pll_freq, struct si5351_reg_set *reg) {
    uint64_t lltmp;
    uint32_t a, b, c, p1, p2, p3;
    uint8_t divby4 = 0;
    uint8_t ret_val = 0;

    // Multisynth bounds checking
    if (freq > SI5351_MULTISYNTH_MAX_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_MULTISYNTH_MAX_FREQ * SI5351_FREQ_MULT;
    }
    if (freq < SI5351_MULTISYNTH_MIN_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_MULTISYNTH_MIN_FREQ * SI5351_FREQ_MULT;
    }

    if (freq >= SI5351_MULTISYNTH_DIVBY4_FREQ * SI5351_FREQ_MULT) {
        divby4 = 1;
    }

    if (pll_freq == 0) {
        // Find largest integer divider for max VCO frequency and given target frequency
        if (divby4 == 0) {
            lltmp = SI5351_PLL_VCO_MAX * SI5351_FREQ_MULT; // margin needed?
            do_div(lltmp, freq);
            if (lltmp == 5) {
                lltmp = 4;
            } else if (lltmp == 7) {
                lltmp = 6;
            }
            a = (uint32_t)lltmp;
        } else {
            a = 4;
        }

        b = 0;
        c = 1;
        pll_freq = a * freq;
    } else {
        // Preset PLL, so return the actual freq for these params instead of PLL freq
        ret_val = 1;

        // Determine integer part of feedback equation
        a = pll_freq / freq;

        if (a < SI5351_MULTISYNTH_A_MIN) {
            freq = pll_freq / SI5351_MULTISYNTH_A_MIN;
        }
        if (a > SI5351_MULTISYNTH_A_MAX) {
            freq = pll_freq / SI5351_MULTISYNTH_A_MAX;
        }

        b = (pll_freq % freq * RFRAC_DENOM) / freq;
        c = b ? RFRAC_DENOM : 1;
    }

    // Calculate parameters
    if (divby4 == 1) {
        p3 = 1;
        p2 = 0;
        p1 = 0;
    } else {
        p1 = 128 * a + ((128 * b) / c) - 512;
        p2 = 128 * b - c * ((128 * b) / c);
        p3 = c;
    }

    reg->p1 = p1;
    reg->p2 = p2;
    reg->p3 = p3;

    if (ret_val == 0) {
        return pll_freq;
    } else {
        return freq;
    }
}

static uint64_t
si5351_multisynth67_calc(uint64_t freq, uint64_t pll_freq, struct si5351_reg_set *reg) {
    uint32_t a;
    uint64_t lltmp;

    // Multisynth bounds checking
    if (freq > SI5351_MULTISYNTH67_MAX_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_MULTISYNTH67_MAX_FREQ * SI5351_FREQ_MULT;
    }
    if (freq < SI5351_MULTISYNTH_MIN_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_MULTISYNTH_MIN_FREQ * SI5351_FREQ_MULT;
    }

    if (pll_freq == 0) {
        // Find largest integer divider for max VCO frequency and given target frequency
        lltmp = (SI5351_PLL_VCO_MAX * SI5351_FREQ_MULT) - 100000000UL; // margin needed?
        do_div(lltmp, freq);
        a = (uint32_t)lltmp;

        // Divisor has to be even
        if (a % 2 != 0) {
            a++;
        }

        // Divisor bounds check
        if (a < SI5351_MULTISYNTH_A_MIN) {
            a = SI5351_MULTISYNTH_A_MIN;
        }
        if (a > SI5351_MULTISYNTH67_A_MAX) {
            a = SI5351_MULTISYNTH67_A_MAX;
        }

        pll_freq = a * freq;

        // PLL bounds checking
        if (pll_freq > (SI5351_PLL_VCO_MAX * SI5351_FREQ_MULT)) {
            a -= 2;
            pll_freq = a * freq;
        } else if (pll_freq < (SI5351_PLL_VCO_MIN * SI5351_FREQ_MULT)) {
            a += 2;
            pll_freq = a * freq;
        }

        reg->p1 = (uint8_t)a;
        reg->p2 = 0;
        reg->p3 = 0;
        return pll_freq;
    } else {
        // Multisynth frequency must be integer division of PLL
        if (pll_freq % freq) {
            // No good
            return 0;
        } else {
            a = pll_freq / freq;

            // Division ratio bounds check
            if (a < SI5351_MULTISYNTH_A_MIN || a > SI5351_MULTISYNTH67_A_MAX) {
                // No bueno
                return 0;
            } else {
                reg->p1 = (uint8_t)a;
                reg->p2 = 0;
                reg->p3 = 0;
                return 1;
            }
        }
    }
}

static void
si5351_update_sys_status(si5351_t *si5351_dev) {
    uint8_t reg_val = 0;

    reg_val = si5351_i2c_read(si5351_dev, SI5351_DEVICE_STATUS);

    // Parse the register
    si5351_dev->si5351_dev_status.SYS_INIT = (reg_val >> 7) & 0x01;
    si5351_dev->si5351_dev_status.LOL_B = (reg_val >> 6) & 0x01;
    si5351_dev->si5351_dev_status.LOL_A = (reg_val >> 5) & 0x01;
    si5351_dev->si5351_dev_status.LOS = (reg_val >> 4) & 0x01;
    si5351_dev->si5351_dev_status.REVID = reg_val & 0x03;
}

static void
si5351_update_int_status(si5351_t *si5351_dev) {
    uint8_t reg_val = 0;

    reg_val = si5351_i2c_read(si5351_dev, SI5351_INTERRUPT_STATUS);

    // Parse the register
    si5351_dev->si5351_dev_int_status.SYS_INIT_STKY = (reg_val >> 7) & 0x01;
    si5351_dev->si5351_dev_int_status.LOL_B_STKY = (reg_val >> 6) & 0x01;
    si5351_dev->si5351_dev_int_status.LOL_A_STKY = (reg_val >> 5) & 0x01;
    si5351_dev->si5351_dev_int_status.LOS_STKY = (reg_val >> 4) & 0x01;
}

static void
si5351_ms_div(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t r_div, uint8_t div_by_4) {
    uint8_t reg_val = 0;
    uint8_t reg_addr = 0;

    switch (clk) {
    case SI5351_CLK0:
        reg_addr = SI5351_CLK0_PARAMETERS + 2;
        break;
    case SI5351_CLK1:
        reg_addr = SI5351_CLK1_PARAMETERS + 2;
        break;
    case SI5351_CLK2:
        reg_addr = SI5351_CLK2_PARAMETERS + 2;
        break;
    case SI5351_CLK3:
        reg_addr = SI5351_CLK3_PARAMETERS + 2;
        break;
    case SI5351_CLK4:
        reg_addr = SI5351_CLK4_PARAMETERS + 2;
        break;
    case SI5351_CLK5:
        reg_addr = SI5351_CLK5_PARAMETERS + 2;
        break;
    case SI5351_CLK6:
        reg_addr = SI5351_CLK6_7_OUTPUT_DIVIDER;
        break;
    case SI5351_CLK7:
        reg_addr = SI5351_CLK6_7_OUTPUT_DIVIDER;
        break;
    }

    reg_val = si5351_i2c_read(si5351_dev, reg_addr);

    if (clk <= (uint8_t)SI5351_CLK5) {
        // Clear the relevant bits
        reg_val &= ~(0x7c);

        if (div_by_4 == 0) {
            reg_val &= ~(SI5351_OUTPUT_CLK_DIVBY4);
        } else {
            reg_val |= (SI5351_OUTPUT_CLK_DIVBY4);
        }

        reg_val |= (r_div << SI5351_OUTPUT_CLK_DIV_SHIFT);
    } else if (clk == SI5351_CLK6) {
        // Clear the relevant bits
        reg_val &= ~(0x07);

        reg_val |= r_div;
    } else if (clk == SI5351_CLK7) {
        // Clear the relevant bits
        reg_val &= ~(0x70);

        reg_val |= (r_div << SI5351_OUTPUT_CLK_DIV_SHIFT);
    }

    si5351_i2c_write(si5351_dev, reg_addr, reg_val);
}

static uint8_t
si5351_select_r_div(uint64_t *freq) {
    uint8_t r_div = SI5351_OUTPUT_CLK_DIV_1;

    // Choose the correct R divider
    if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT) &&
        (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 2)) {
        r_div = SI5351_OUTPUT_CLK_DIV_128;
        *freq *= 128ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 2) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 4)) {
        r_div = SI5351_OUTPUT_CLK_DIV_64;
        *freq *= 64ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 4) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 8)) {
        r_div = SI5351_OUTPUT_CLK_DIV_32;
        *freq *= 32ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 8) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 16)) {
        r_div = SI5351_OUTPUT_CLK_DIV_16;
        *freq *= 16ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 16) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 32)) {
        r_div = SI5351_OUTPUT_CLK_DIV_8;
        *freq *= 8ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 32) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 64)) {
        r_div = SI5351_OUTPUT_CLK_DIV_4;
        *freq *= 4ULL;
    } else if ((*freq >= SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 64) &&
               (*freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT * 128)) {
        r_div = SI5351_OUTPUT_CLK_DIV_2;
        *freq *= 2ULL;
    }

    return r_div;
}

static uint8_t
si5351_select_r_div_ms67(uint64_t *freq) {
    uint8_t r_div = SI5351_OUTPUT_CLK_DIV_1;

    // Choose the correct R divider
    if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT) &&
        (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 2)) {
        r_div = SI5351_OUTPUT_CLK_DIV_128;
        *freq *= 128ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 2) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 4)) {
        r_div = SI5351_OUTPUT_CLK_DIV_64;
        *freq *= 64ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 4) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 8)) {
        r_div = SI5351_OUTPUT_CLK_DIV_32;
        *freq *= 32ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 8) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 16)) {
        r_div = SI5351_OUTPUT_CLK_DIV_16;
        *freq *= 16ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 16) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 32)) {
        r_div = SI5351_OUTPUT_CLK_DIV_8;
        *freq *= 8ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 32) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 64)) {
        r_div = SI5351_OUTPUT_CLK_DIV_4;
        *freq *= 4ULL;
    } else if ((*freq >= SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 64) &&
               (*freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT * 128)) {
        r_div = SI5351_OUTPUT_CLK_DIV_2;
        *freq *= 2ULL;
    }

    return r_div;
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t
si5351_init(si5351_t *si5351_dev, uint8_t xtal_load_c, uint32_t xo_freq, int32_t corr) {
    uint8_t reg_val = 0;

    si5351_dev->si5351_dev_status.SYS_INIT = 0;
    si5351_dev->si5351_dev_status.LOL_B = 0;
    si5351_dev->si5351_dev_status.LOL_A = 0;
    si5351_dev->si5351_dev_status.LOS = 0;
    si5351_dev->si5351_dev_status.REVID = 0;

    si5351_dev->si5351_dev_int_status.SYS_INIT_STKY = 0;
    si5351_dev->si5351_dev_int_status.LOL_B_STKY = 0;
    si5351_dev->si5351_dev_int_status.LOL_A_STKY = 0;
    si5351_dev->si5351_dev_int_status.LOS_STKY = 0;

    si5351_dev->si5351_xtal_freq[0] = SI5351_XTAL_FREQ;

    // Start by using XO ref osc as default for each PLL
    si5351_dev->plla_ref_osc = SI5351_PLL_INPUT_XO;
    si5351_dev->pllb_ref_osc = SI5351_PLL_INPUT_XO;
    si5351_dev->si5351_clkin_div = SI5351_CLKIN_DIV_1;

    if (reg_val == 0) {
        // Wait for SYS_INIT flag to be clear, indicating that device is ready
        uint8_t status_reg = 0;
        do {
            status_reg = si5351_i2c_read(si5351_dev, SI5351_DEVICE_STATUS);
        } while (status_reg >> 7 == 1);

        // Set crystal load capacitance
        si5351_i2c_write(si5351_dev, SI5351_CRYSTAL_LOAD, (xtal_load_c & SI5351_CRYSTAL_LOAD_MASK) | 0b00010010);

        // Set up the XO reference frequency
        if (xo_freq != 0) {
            si5351_set_ref_freq(si5351_dev, xo_freq, SI5351_PLL_INPUT_XO);
        } else {
            si5351_set_ref_freq(si5351_dev, SI5351_XTAL_FREQ, SI5351_PLL_INPUT_XO);
        }

        // Set the frequency calibration for the XO
        si5351_set_correction(si5351_dev, corr, SI5351_PLL_INPUT_XO);
        si5351_reset(si5351_dev);

        return ESP_OK;
    }

    return ESP_FAIL;
}

void
si5351_reset(si5351_t *si5351_dev) {
    // Initialize the CLK outputs according to flowchart in datasheet
    // First, turn them off
    si5351_i2c_write(si5351_dev, 16, 0x80);
    si5351_i2c_write(si5351_dev, 17, 0x80);
    si5351_i2c_write(si5351_dev, 18, 0x80);
    si5351_i2c_write(si5351_dev, 19, 0x80);
    si5351_i2c_write(si5351_dev, 20, 0x80);
    si5351_i2c_write(si5351_dev, 21, 0x80);
    si5351_i2c_write(si5351_dev, 22, 0x80);
    si5351_i2c_write(si5351_dev, 23, 0x80);

    // Turn the clocks back on...
    si5351_i2c_write(si5351_dev, 16, 0x0c);
    si5351_i2c_write(si5351_dev, 17, 0x0c);
    si5351_i2c_write(si5351_dev, 18, 0x0c);
    si5351_i2c_write(si5351_dev, 19, 0x0c);
    si5351_i2c_write(si5351_dev, 20, 0x0c);
    si5351_i2c_write(si5351_dev, 21, 0x0c);
    si5351_i2c_write(si5351_dev, 22, 0x0c);
    si5351_i2c_write(si5351_dev, 23, 0x0c);

    // Set PLLA and PLLB to 800 MHz for automatic tuning
    si5351_set_pll(si5351_dev, SI5351_PLL_FIXED, SI5351_PLLA);
    si5351_set_pll(si5351_dev, SI5351_PLL_FIXED, SI5351_PLLB);

    // Make PLL to CLK assignments for automatic tuning
    si5351_dev->pll_assignment[0] = SI5351_PLLA;
    si5351_dev->pll_assignment[1] = SI5351_PLLA;
    si5351_dev->pll_assignment[2] = SI5351_PLLA;
    si5351_dev->pll_assignment[3] = SI5351_PLLA;
    si5351_dev->pll_assignment[4] = SI5351_PLLA;
    si5351_dev->pll_assignment[5] = SI5351_PLLA;
    si5351_dev->pll_assignment[6] = SI5351_PLLB;
    si5351_dev->pll_assignment[7] = SI5351_PLLB;

    si5351_set_ms_source(si5351_dev, SI5351_CLK0, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK1, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK2, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK3, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK4, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK5, SI5351_PLLA);
    si5351_set_ms_source(si5351_dev, SI5351_CLK6, SI5351_PLLB);
    si5351_set_ms_source(si5351_dev, SI5351_CLK7, SI5351_PLLB);

    // Reset the VCXO param
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_LOW, 0);
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_MID, 0);
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_HIGH, 0);

    // Then reset the PLLs
    si5351_pll_reset(si5351_dev, SI5351_PLLA);
    si5351_pll_reset(si5351_dev, SI5351_PLLB);

    // Set initial frequencies
    uint8_t i;
    for (i = 0; i < 8; i++) {
        si5351_dev->si5351_clk_freq[i] = 0;
        si5351_output_enable(si5351_dev, (enum si5351_clock)i, 0);
        si5351_dev->si5351_clk_first_set[i] = false;
    }
}

void
si5351_fast_reset(si5351_t *si5351_dev) {
    // This soft-resets PLL A & B (32 + 128) in just one step
    si5351_i2c_write(si5351_dev, SI5351_PLL_RESET, 0xA0);
}

bool
si5351_set_freq(si5351_t *si5351_dev, uint64_t freq, enum si5351_clock clk) {
    struct si5351_reg_set ms_reg;
    uint64_t pll_freq;
    uint8_t int_mode = 0;
    uint8_t div_by_4 = 0;
    uint8_t r_div = 0;

    // Check which Multisynth is being set
    if ((uint8_t)clk <= (uint8_t)SI5351_CLK5) {
        // MS0 through MS5 logic
        // ---------------------

        // Lower bounds check
        if (freq > 0 && freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT) {
            freq = SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT;
        }

        // Upper bounds check
        if (freq > SI5351_MULTISYNTH_MAX_FREQ * SI5351_FREQ_MULT) {
            freq = SI5351_MULTISYNTH_MAX_FREQ * SI5351_FREQ_MULT;
        }

        // If requested freq >100 MHz and no other outputs are already >100 MHz,
        // we need to recalculate PLLA and then recalculate all other CLK outputs on same PLL
        if (freq > (SI5351_MULTISYNTH_SHARE_MAX * SI5351_FREQ_MULT)) {
            // Check other clocks on same PLL
            uint8_t i;
            for (i = 0; i < 6; i++) {
                if (si5351_dev->si5351_clk_freq[i] > (SI5351_MULTISYNTH_SHARE_MAX * SI5351_FREQ_MULT)) {
                    if (i != (uint8_t)clk && si5351_dev->pll_assignment[i] == si5351_dev->pll_assignment[clk]) {
                        return 1; // won't set if any other clks already >100 MHz
                    }
                }
            }

            // Enable the output on first set_freq only
            if (si5351_dev->si5351_clk_first_set[(uint8_t)clk] == false) {
                si5351_output_enable(si5351_dev, clk, 1);
                si5351_dev->si5351_clk_first_set[(uint8_t)clk] = true;
            }

            // Set the freq in memory
            si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

            // Calculate the proper PLL frequency
            pll_freq = si5351_multisynth_calc(freq, 0, &ms_reg);

            // Set PLL
            si5351_set_pll(si5351_dev, pll_freq, si5351_dev->pll_assignment[clk]);

            // Recalculate params for other synths on same PLL
            for (i = 0; i < 6; i++) {
                if (si5351_dev->si5351_clk_freq[i] != 0) {
                    if (si5351_dev->pll_assignment[i] == si5351_dev->pll_assignment[clk]) {
                        struct si5351_reg_set temp_reg;
                        uint64_t temp_freq;

                        // Select the proper R div value
                        temp_freq = si5351_dev->si5351_clk_freq[i];
                        r_div = si5351_select_r_div(&temp_freq);

                        si5351_multisynth_calc(temp_freq, pll_freq, &temp_reg);

                        // If freq > 150 MHz, we need to use DIVBY4 and integer mode
                        if (temp_freq >= SI5351_MULTISYNTH_DIVBY4_FREQ * SI5351_FREQ_MULT) {
                            div_by_4 = 1;
                            int_mode = 1;
                        } else {
                            div_by_4 = 0;
                            int_mode = 0;
                        }

                        // Set multisynth registers
                        si5351_set_ms(si5351_dev, (enum si5351_clock)i, temp_reg, int_mode, r_div, div_by_4);
                    }
                }
            }

            // Reset the PLL
            si5351_pll_reset(si5351_dev, si5351_dev->pll_assignment[clk]);
        } else {
            si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

            // Enable the output on first set_freq only
            if (si5351_dev->si5351_clk_first_set[(uint8_t)clk] == false) {
                si5351_output_enable(si5351_dev, clk, 1);
                si5351_dev->si5351_clk_first_set[(uint8_t)clk] = true;
            }

            // Select the proper R div value
            r_div = si5351_select_r_div(&freq);

            // Calculate the synth parameters
            if (si5351_dev->pll_assignment[clk] == SI5351_PLLA) {
                si5351_multisynth_calc(freq, si5351_dev->si5351_plla_freq, &ms_reg);
            } else {
                si5351_multisynth_calc(freq, si5351_dev->si5351_pllb_freq, &ms_reg);
            }

            // Set multisynth registers
            si5351_set_ms(si5351_dev, clk, ms_reg, int_mode, r_div, div_by_4);

            // Reset the PLL
            // si5351_pll_reset(pll_assignment[clk]);
        }

        return true;
    } else {
        // MS6 and MS7 logic
        // -----------------

        // Lower bounds check
        if (freq > 0 && freq < SI5351_CLKOUT67_MIN_FREQ * SI5351_FREQ_MULT) {
            freq = SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT;
        }

        // Upper bounds check
        if (freq >= SI5351_MULTISYNTH_DIVBY4_FREQ * SI5351_FREQ_MULT) {
            freq = SI5351_MULTISYNTH_DIVBY4_FREQ * SI5351_FREQ_MULT - 1;
        }

        // If one of CLK6 or CLK7 is already set when trying to set the other, we have to ensure that it will also have
        // an integer division ratio with the same PLL, otherwise do not set it.
        if (clk == SI5351_CLK6) {
            if (si5351_dev->si5351_clk_freq[7] != 0) {
                if (si5351_dev->si5351_pllb_freq % freq == 0) {
                    if ((si5351_dev->si5351_pllb_freq / freq) % 2 != 0) {
                        // Not an even divide ratio, no bueno
                        return false;
                    } else {
                        // Set the freq in memory
                        si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

                        // Select the proper R div value
                        r_div = si5351_select_r_div_ms67(&freq);

                        si5351_multisynth67_calc(freq, si5351_dev->si5351_pllb_freq, &ms_reg);
                    }
                } else {
                    // Not an integer divide ratio, no good
                    return false;
                }
            } else {
                // No previous assignment, so set PLLB based on CLK6

                // Set the freq in memory
                si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

                // Select the proper R div value
                r_div = si5351_select_r_div_ms67(&freq);

                pll_freq = si5351_multisynth67_calc(freq, 0, &ms_reg);
                // pllb_freq = pll_freq;
                si5351_set_pll(si5351_dev, pll_freq, SI5351_PLLB);
            }
        } else {
            if (si5351_dev->si5351_clk_freq[6] != 0) {
                if (si5351_dev->si5351_pllb_freq % freq == 0) {
                    if ((si5351_dev->si5351_pllb_freq / freq) % 2 != 0) {
                        // Not an even divide ratio, no bueno
                        return false;
                    } else {
                        // Set the freq in memory
                        si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

                        // Select the proper R div value
                        r_div = si5351_select_r_div_ms67(&freq);

                        si5351_multisynth67_calc(freq, si5351_dev->si5351_pllb_freq, &ms_reg);
                    }
                } else {
                    // Not an integer divide ratio, no good
                    return false;
                }
            } else {
                // No previous assignment, so set PLLB based on CLK7

                // Set the freq in memory
                si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

                // Select the proper R div value
                r_div = si5351_select_r_div_ms67(&freq);

                pll_freq = si5351_multisynth67_calc(freq, 0, &ms_reg);
                // pllb_freq = pll_freq;
                si5351_set_pll(si5351_dev, pll_freq, si5351_dev->pll_assignment[clk]);
            }
        }

        div_by_4 = 0;
        int_mode = 0;

        // Set multisynth registers (MS must be set before PLL)
        si5351_set_ms(si5351_dev, clk, ms_reg, int_mode, r_div, div_by_4);

        return true;
    }
}

bool
si5351_set_freq_manual(si5351_t *si5351_dev, uint64_t freq, uint64_t pll_freq, enum si5351_clock clk) {
    struct si5351_reg_set ms_reg;
    uint8_t int_mode = 0;
    uint8_t div_by_4 = 0;

    // Lower bounds check
    if (freq > 0 && freq < SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_CLKOUT_MIN_FREQ * SI5351_FREQ_MULT;
    }

    // Upper bounds check
    if (freq > SI5351_CLKOUT_MAX_FREQ * SI5351_FREQ_MULT) {
        freq = SI5351_CLKOUT_MAX_FREQ * SI5351_FREQ_MULT;
    }

    uint8_t r_div;

    si5351_dev->si5351_clk_freq[(uint8_t)clk] = freq;

    si5351_set_pll(si5351_dev, pll_freq, si5351_dev->pll_assignment[clk]);

    // Enable the output
    si5351_output_enable(si5351_dev, clk, 1);

    // Select the proper R div value
    r_div = si5351_select_r_div(&freq);

    // Calculate the synth parameters
    si5351_multisynth_calc(freq, pll_freq, &ms_reg);

    // If freq > 150 MHz, we need to use DIVBY4 and integer mode
    if (freq >= SI5351_MULTISYNTH_DIVBY4_FREQ * SI5351_FREQ_MULT) {
        div_by_4 = 1;
        int_mode = 1;
    }

    // Set multisynth registers (MS must be set before PLL)
    si5351_set_ms(si5351_dev, clk, ms_reg, int_mode, r_div, div_by_4);

    return true;
}

void
si5351_set_pll(si5351_t *si5351_dev, uint64_t pll_freq, enum si5351_pll target_pll) {
    struct si5351_reg_set pll_reg;

    if (target_pll == SI5351_PLLA) {
        si5351_pll_calc(si5351_dev, SI5351_PLLA, pll_freq, &pll_reg,
                        si5351_dev->si5351_ref_correction[si5351_dev->plla_ref_osc], 0);
    } else {
        si5351_pll_calc(si5351_dev, SI5351_PLLB, pll_freq, &pll_reg,
                        si5351_dev->si5351_ref_correction[si5351_dev->pllb_ref_osc], 0);
    }

    // Derive the register values to write

    // Prepare an array for parameters to be written to
    uint8_t params[20];
    uint8_t i = 0;
    uint8_t temp;

    // Registers 26-27
    temp = ((pll_reg.p3 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p3 & 0xFF);
    params[i++] = temp;

    // Register 28
    temp = (uint8_t)((pll_reg.p1 >> 16) & 0x03);
    params[i++] = temp;

    // Registers 29-30
    temp = (uint8_t)((pll_reg.p1 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p1 & 0xFF);
    params[i++] = temp;

    // Register 31
    temp = (uint8_t)((pll_reg.p3 >> 12) & 0xF0);
    temp += (uint8_t)((pll_reg.p2 >> 16) & 0x0F);
    params[i++] = temp;

    // Registers 32-33
    temp = (uint8_t)((pll_reg.p2 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p2 & 0xFF);
    params[i++] = temp;

    // Write the parameters
    if (target_pll == SI5351_PLLA) {
        si5351_i2c_write_bulk(si5351_dev, SI5351_PLLA_PARAMETERS, i, params);
        si5351_dev->si5351_plla_freq = pll_freq;
    } else if (target_pll == SI5351_PLLB) {
        si5351_i2c_write_bulk(si5351_dev, SI5351_PLLB_PARAMETERS, i, params);
        si5351_dev->si5351_pllb_freq = pll_freq;
    }
}

void
si5351_set_ms(si5351_t *si5351_dev, enum si5351_clock clk, struct si5351_reg_set ms_reg, uint8_t int_mode,
              uint8_t r_div, uint8_t div_by_4) {
    uint8_t params[20];
    uint8_t i = 0;
    uint8_t temp;
    uint8_t reg_val;

    if ((uint8_t)clk <= (uint8_t)SI5351_CLK5) {
        // Registers 42-43 for CLK0
        temp = (uint8_t)((ms_reg.p3 >> 8) & 0xFF);
        params[i++] = temp;

        temp = (uint8_t)(ms_reg.p3 & 0xFF);
        params[i++] = temp;

        // Register 44 for CLK0
        reg_val = si5351_i2c_read(si5351_dev, (SI5351_CLK0_PARAMETERS + 2) + (clk * 8));
        reg_val &= ~(0x03);
        temp = reg_val | ((uint8_t)((ms_reg.p1 >> 16) & 0x03));
        params[i++] = temp;

        // Registers 45-46 for CLK0
        temp = (uint8_t)((ms_reg.p1 >> 8) & 0xFF);
        params[i++] = temp;

        temp = (uint8_t)(ms_reg.p1 & 0xFF);
        params[i++] = temp;

        // Register 47 for CLK0
        temp = (uint8_t)((ms_reg.p3 >> 12) & 0xF0);
        temp += (uint8_t)((ms_reg.p2 >> 16) & 0x0F);
        params[i++] = temp;

        // Registers 48-49 for CLK0
        temp = (uint8_t)((ms_reg.p2 >> 8) & 0xFF);
        params[i++] = temp;

        temp = (uint8_t)(ms_reg.p2 & 0xFF);
        params[i++] = temp;
    } else {
        // MS6 and MS7 only use one register
        temp = ms_reg.p1;
    }

    // Write the parameters
    switch (clk) {
    case SI5351_CLK0:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK0_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK1:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK1_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK2:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK2_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK3:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK3_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK4:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK4_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK5:
        si5351_i2c_write_bulk(si5351_dev, SI5351_CLK5_PARAMETERS, i, params);
        si5351_set_int(si5351_dev, clk, int_mode);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK6:
        si5351_i2c_write(si5351_dev, SI5351_CLK6_PARAMETERS, temp);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    case SI5351_CLK7:
        si5351_i2c_write(si5351_dev, SI5351_CLK7_PARAMETERS, temp);
        si5351_ms_div(si5351_dev, clk, r_div, div_by_4);
        break;
    }
}

void
si5351_output_enable(si5351_t *si5351_dev, enum si5351_clock clk, bool enable) {
    uint8_t reg_val;

    reg_val = si5351_i2c_read(si5351_dev, SI5351_OUTPUT_ENABLE_CTRL);

    if (enable) {
        reg_val &= ~(1 << (uint8_t)clk);
    } else {
        reg_val |= (1 << (uint8_t)clk);
    }

    si5351_i2c_write(si5351_dev, SI5351_OUTPUT_ENABLE_CTRL, reg_val);
}

void
si5351_drive_strength(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_drive drive) {
    uint8_t reg_val;
    const uint8_t mask = 0x03;

    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);
    reg_val &= ~(mask);

    switch (drive) {
    case SI5351_DRIVE_2MA:
        reg_val |= 0x00;
        break;
    case SI5351_DRIVE_4MA:
        reg_val |= 0x01;
        break;
    case SI5351_DRIVE_6MA:
        reg_val |= 0x02;
        break;
    case SI5351_DRIVE_8MA:
        reg_val |= 0x03;
        break;
    default:
        break;
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);
}

void
si5351_update_status(si5351_t *si5351_dev) {
    si5351_update_sys_status(si5351_dev);
    si5351_update_int_status(si5351_dev);
}

void
si5351_set_correction(si5351_t *si5351_dev, int32_t corr, enum si5351_pll_input ref_osc) {
    si5351_dev->si5351_ref_correction[(uint8_t)ref_osc] = corr;

    // Recalculate and set PLL freqs based on correction value
    si5351_set_pll(si5351_dev, si5351_dev->si5351_plla_freq, SI5351_PLLA);
    ;
    si5351_set_pll(si5351_dev, si5351_dev->si5351_pllb_freq, SI5351_PLLB);
}

void
si5351_set_phase(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t phase) {
    // Mask off the upper bit since it is reserved
    phase = phase & 0b01111111;

    si5351_i2c_write(si5351_dev, SI5351_CLK0_PHASE_OFFSET + (uint8_t)clk, phase);
}

int32_t
si5351_get_correction(si5351_t *si5351_dev, enum si5351_pll_input ref_osc) {
    return si5351_dev->si5351_ref_correction[(uint8_t)ref_osc];
}

void
si5351_pll_reset(si5351_t *si5351_dev, enum si5351_pll target_pll) {
    if (target_pll == SI5351_PLLA) {
        si5351_i2c_write(si5351_dev, SI5351_PLL_RESET, SI5351_PLL_RESET_A);
    } else if (target_pll == SI5351_PLLB) {
        si5351_i2c_write(si5351_dev, SI5351_PLL_RESET, SI5351_PLL_RESET_B);
    }
}

void
si5351_set_ms_source(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_pll pll) {
    uint8_t reg_val;

    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);

    if (pll == SI5351_PLLA) {
        reg_val &= ~(SI5351_CLK_PLL_SELECT);
    } else if (pll == SI5351_PLLB) {
        reg_val |= SI5351_CLK_PLL_SELECT;
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);

    si5351_dev->pll_assignment[(uint8_t)clk] = pll;
}

void
si5351_set_int(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t enable) {
    uint8_t reg_val;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);

    if (enable == 1) {
        reg_val |= (SI5351_CLK_INTEGER_MODE);
    } else {
        reg_val &= ~(SI5351_CLK_INTEGER_MODE);
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);

    // Integer mode indication
    /*
     switch(clk) {
         case SI5351_CLK0:
             clk0_int_mode = enable;
             break;
         case SI5351_CLK1:
             clk1_int_mode = enable;
             break;
         case SI5351_CLK2:
             clk2_int_mode = enable;
             break;
         default:
             break;
     }
     */
}

void
si5351_set_clock_pwr(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t pwr) {
    uint8_t reg_val; //, reg;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);

    if (pwr == 1) {
        reg_val &= 0b01111111;
    } else {
        reg_val |= 0b10000000;
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);
}

void
si5351_set_clock_invert(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t inv) {
    uint8_t reg_val;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);

    if (inv == 1) {
        reg_val |= (SI5351_CLK_INVERT);
    } else {
        reg_val &= ~(SI5351_CLK_INVERT);
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);
}

void
si5351_set_clock_source(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_clock_source src) {
    uint8_t reg_val;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk);

    // Clear the bits first
    reg_val &= ~(SI5351_CLK_INPUT_MASK);

    switch (src) {
    case SI5351_CLK_SRC_XTAL:
        reg_val |= (SI5351_CLK_INPUT_XTAL);
        break;
    case SI5351_CLK_SRC_CLKIN:
        reg_val |= (SI5351_CLK_INPUT_CLKIN);
        break;
    case SI5351_CLK_SRC_MS0:
        if (clk == SI5351_CLK0) {
            return;
        }

        reg_val |= (SI5351_CLK_INPUT_MULTISYNTH_0_4);
        break;
    case SI5351_CLK_SRC_MS:
        reg_val |= (SI5351_CLK_INPUT_MULTISYNTH_N);
        break;
    default:
        return;
    }

    si5351_i2c_write(si5351_dev, SI5351_CLK0_CTRL + (uint8_t)clk, reg_val);
}

void
si5351_set_clock_disable(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_clock_disable dis_state) {
    uint8_t reg_val, reg = SI5351_CLK3_0_DISABLE_STATE;

    if (clk >= SI5351_CLK0 && clk <= SI5351_CLK3) {
        reg = SI5351_CLK3_0_DISABLE_STATE;
    } else if (clk >= SI5351_CLK4 && clk <= SI5351_CLK7) {
        reg = SI5351_CLK7_4_DISABLE_STATE;
    } else
        return;

    reg_val = si5351_i2c_read(si5351_dev, reg);

    if (clk >= SI5351_CLK0 && clk <= SI5351_CLK3) {
        reg_val &= ~(0b11 << (clk * 2));
        reg_val |= dis_state << (clk * 2);
    } else if (clk >= SI5351_CLK4 && clk <= SI5351_CLK7) {
        reg_val &= ~(0b11 << ((clk - 4) * 2));
        reg_val |= dis_state << ((clk - 4) * 2);
    }

    si5351_i2c_write(si5351_dev, reg, reg_val);
}

void
si5351_set_clock_fanout(si5351_t *si5351_dev, enum si5351_clock_fanout fanout, uint8_t enable) {
    uint8_t reg_val;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_FANOUT_ENABLE);

    switch (fanout) {
    case SI5351_FANOUT_CLKIN:
        if (enable) {
            reg_val |= SI5351_CLKIN_ENABLE;
        } else {
            reg_val &= ~(SI5351_CLKIN_ENABLE);
        }
        break;
    case SI5351_FANOUT_XO:
        if (enable) {
            reg_val |= SI5351_XTAL_ENABLE;
        } else {
            reg_val &= ~(SI5351_XTAL_ENABLE);
        }
        break;
    case SI5351_FANOUT_MS:
        if (enable) {
            reg_val |= SI5351_MULTISYNTH_ENABLE;
        } else {
            reg_val &= ~(SI5351_MULTISYNTH_ENABLE);
        }
        break;
    }

    si5351_i2c_write(si5351_dev, SI5351_FANOUT_ENABLE, reg_val);
}

void
si5351_set_pll_input(si5351_t *si5351_dev, enum si5351_pll pll, enum si5351_pll_input input) {
    uint8_t reg_val;
    reg_val = si5351_i2c_read(si5351_dev, SI5351_PLL_INPUT_SOURCE);

    // Clear the bits first
    // reg_val &= ~(SI5351_CLKIN_DIV_MASK);

    switch (pll) {
    case SI5351_PLLA:
        if (input == SI5351_PLL_INPUT_CLKIN) {
            reg_val |= SI5351_PLLA_SOURCE;
            reg_val |= si5351_dev->si5351_clkin_div;
            si5351_dev->plla_ref_osc = SI5351_PLL_INPUT_CLKIN;
        } else {
            reg_val &= ~(SI5351_PLLA_SOURCE);
            si5351_dev->plla_ref_osc = SI5351_PLL_INPUT_XO;
        }
        break;
    case SI5351_PLLB:
        if (input == SI5351_PLL_INPUT_CLKIN) {
            reg_val |= SI5351_PLLB_SOURCE;
            reg_val |= si5351_dev->si5351_clkin_div;
            si5351_dev->pllb_ref_osc = SI5351_PLL_INPUT_CLKIN;
        } else {
            reg_val &= ~(SI5351_PLLB_SOURCE);
            si5351_dev->pllb_ref_osc = SI5351_PLL_INPUT_XO;
        }
        break;
    default:
        return;
    }

    si5351_i2c_write(si5351_dev, SI5351_PLL_INPUT_SOURCE, reg_val);

    si5351_set_pll(si5351_dev, si5351_dev->si5351_plla_freq, SI5351_PLLA);
    si5351_set_pll(si5351_dev, si5351_dev->si5351_pllb_freq, SI5351_PLLB);
}

void
si5351_set_vcxo(si5351_t *si5351_dev, uint64_t pll_freq, uint8_t ppm) {
    struct si5351_reg_set pll_reg;
    uint64_t vcxo_param;

    // Bounds check
    if (ppm < SI5351_VCXO_PULL_MIN) {
        ppm = SI5351_VCXO_PULL_MIN;
    }

    if (ppm > SI5351_VCXO_PULL_MAX) {
        ppm = SI5351_VCXO_PULL_MAX;
    }

    // Set PLLB params
    vcxo_param = si5351_pll_calc(si5351_dev, SI5351_PLLB, pll_freq, &pll_reg,
                                 si5351_dev->si5351_ref_correction[si5351_dev->pllb_ref_osc], 1);

    // Derive the register values to write

    // Prepare an array for parameters to be written to
    uint8_t params[20];
    uint8_t i = 0;
    uint8_t temp;

    // Registers 26-27
    temp = ((pll_reg.p3 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p3 & 0xFF);
    params[i++] = temp;

    // Register 28
    temp = (uint8_t)((pll_reg.p1 >> 16) & 0x03);
    params[i++] = temp;

    // Registers 29-30
    temp = (uint8_t)((pll_reg.p1 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p1 & 0xFF);
    params[i++] = temp;

    // Register 31
    temp = (uint8_t)((pll_reg.p3 >> 12) & 0xF0);
    temp += (uint8_t)((pll_reg.p2 >> 16) & 0x0F);
    params[i++] = temp;

    // Registers 32-33
    temp = (uint8_t)((pll_reg.p2 >> 8) & 0xFF);
    params[i++] = temp;

    temp = (uint8_t)(pll_reg.p2 & 0xFF);
    params[i++] = temp;

    // Write the parameters
    si5351_i2c_write_bulk(si5351_dev, SI5351_PLLB_PARAMETERS, i, params);

    // Write the VCXO parameters
    vcxo_param = ((vcxo_param * ppm * SI5351_VCXO_MARGIN) / 100ULL) / 1000000ULL;

    temp = (uint8_t)(vcxo_param & 0xFF);
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_LOW, temp);

    temp = (uint8_t)((vcxo_param >> 8) & 0xFF);
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_MID, temp);

    temp = (uint8_t)((vcxo_param >> 16) & 0x3F);
    si5351_i2c_write(si5351_dev, SI5351_VXCO_PARAMETERS_HIGH, temp);
}

void
si5351_set_ref_freq(si5351_t *si5351_dev, uint32_t ref_freq, enum si5351_pll_input ref_osc) {
    // uint8_t reg_val;
    // reg_val = si5351_read(SI5351_PLL_INPUT_SOURCE);

    // Clear the bits first
    // reg_val &= ~(SI5351_CLKIN_DIV_MASK);

    if (ref_freq <= 30000000UL) {
        si5351_dev->si5351_xtal_freq[(uint8_t)ref_osc] = ref_freq;
        // reg_val |= SI5351_CLKIN_DIV_1;
        if (ref_osc == SI5351_PLL_INPUT_CLKIN) {
            si5351_dev->si5351_clkin_div = SI5351_CLKIN_DIV_1;
        }
    } else if (ref_freq > 30000000UL && ref_freq <= 60000000UL) {
        si5351_dev->si5351_xtal_freq[(uint8_t)ref_osc] = ref_freq / 2;
        // reg_val |= SI5351_CLKIN_DIV_2;
        if (ref_osc == SI5351_PLL_INPUT_CLKIN) {
            si5351_dev->si5351_clkin_div = SI5351_CLKIN_DIV_2;
        }
    } else if (ref_freq > 60000000UL && ref_freq <= 100000000UL) {
        si5351_dev->si5351_xtal_freq[(uint8_t)ref_osc] = ref_freq / 4;
        // reg_val |= SI5351_CLKIN_DIV_4;
        if (ref_osc == SI5351_PLL_INPUT_CLKIN) {
            si5351_dev->si5351_clkin_div = SI5351_CLKIN_DIV_4;
        }
    } else {
        // reg_val |= SI5351_CLKIN_DIV_1;
    }

    // si5351_write(SI5351_PLL_INPUT_SOURCE, reg_val);
}

void
si5351_spread_spectrum(si5351_t *si5351_dev, bool enabled) {
    uint8_t regval = si5351_i2c_read(si5351_dev, SI5351_SSC_PARAM0);
    if (enabled) {
        regval |= 0x80;
    } else {
        regval &= ~0x80;
    }
    si5351_i2c_write(si5351_dev, SI5351_SSC_PARAM0, regval);
}

void
si5351_set_freq2(si5351_t *si5351_dev, uint64_t freq, enum si5351_clock clk) {
    // this is the work value, with the correction applied via the correction()
    // procedure
    uint32_t int_xtal = SI5351_XTAL_FREQ;

    // BAD CONCEPT on the datasheet and AN:
    // The chip has a soft-reset for PLL A & B but in practice the PLL does not need to be reseted. Test shows that if
    // you fix the Msynth output dividers and move any of the VCO from bottom to top the frequency moves smooth and
    // clean, no reset needed The reset is needed when you changes the value of the Msynth output divider, even so it's
    // not always needed so we use this var to keep track of all three and only reset the "PLL" when this value changes
    // to be sure It's a word (16 bit) because the final max value is 900
    uint16_t omsynth[3] = {0};
    uint8_t o_rdiv[3] = {0};

    uint8_t a, r = 1, pll_stride = 0, msyn_stride = 0;
    uint32_t b, c, f, fvco, outdivider;
    uint32_t msx_p1, msnx_p1, msnx_p2, msnx_p3;

    // Overclock option
#ifdef SI_OVERCLOCK
    // user a overclock setting for the VCO, max value in my hardware was 1.05 to 1.1 GHz, as usual YMMV
    outdivider = SI_OVERCLOCK / freq;
#else
    // normal VCO from the datasheet and AN, with 900 MHz being the maximum internal PLL-Frequency
    outdivider = 900000000 / freq;
#endif

    // use additional Output divider ("R")
    while (outdivider > 900) {
        r = r * 2;
        outdivider = outdivider / 2;
    }

    // finds the even divider which delivers the intended Frequency
    if (outdivider % 2)
        outdivider--;

    // Calculate the PLL-Frequency (given the even divider)
    fvco = outdivider * r * freq;

    // Convert the Output Divider to the bit-setting required in register 44
    switch (r) {
    case 1:
        r = 0;
        break;
    case 2:
        r = 16;
        break;
    case 4:
        r = 32;
        break;
    case 8:
        r = 48;
        break;
    case 16:
        r = 64;
        break;
    case 32:
        r = 80;
        break;
    case 64:
        r = 96;
        break;
    case 128:
        r = 112;
        break;
    }

    // we have now the integer part of the output msynth the b & c is fixed below
    msx_p1 = 128 * outdivider - 512;

    // calc the a/b/c for the PLL Msynth
    // We will use integer only on the b/c relation, and will >> 5 (/32) both to fit it on the 1048 k limit of C and
    // keep the relation the most accurate possible, this works fine with xtals from 24 to 28 Mhz. This will give errors
    // of about +/- 2 Hz maximum as per my test and simulations in the worst case, well below the XTAl ppm error... This
    // will free more than 1K of the final eeprom
    a = fvco / int_xtal;
    b = (fvco % int_xtal) >> 5; // Integer part of the fraction scaled to match "c" limits
    c = int_xtal >> 5;          // "c" scaled to match it's limits in the register

    // f is (128*b)/c to mimic the Floor(128*(b/c)) from the datasheet
    f = (128 * b) / c;

    // build the registers to write
    msnx_p1 = 128 * a + f - 512;
    msnx_p2 = 128 * b - f * c;
    msnx_p3 = c;

    // PLLs and CLK# registers are allocated with a stride, we handle that with the stride var to make code smaller
    if (clk > 0)
        pll_stride = 8;

    // HEX makes it easier to human read on bit shifts
    uint8_t reg_bank_26[] = {
        (msnx_p3 & 0xFF00) >> 8, // Bits [15:8] of MSNx_P3 in register 26
        msnx_p3 & 0xFF,
        (msnx_p1 & 0x030000L) >> 16,
        (msnx_p1 & 0xFF00) >> 8,                                      // Bits [15:8] of MSNx_P1 in register 29
        msnx_p1 & 0xFF,                                               // Bits [7:0]  of MSNx_P1 in register 30
        ((msnx_p3 & 0x0F0000L) >> 12) | ((msnx_p2 & 0x0F0000) >> 16), // Parts       of MSNx_P3 and MSNx_P1
        (msnx_p2 & 0xFF00) >> 8,                                      // Bits [15:8] of MSNx_P2 in register 32
        msnx_p2 & 0xFF                                                // Bits [7:0]  of MSNx_P2 in register 33
    };

    // We could do this here - but move it next to the reg_bank_42
    // Write the output divider msynth only if we need to, in this way we can speed up the frequency changes almost by
    // half the time most of the time and the main goal is to avoid the nasty click noise on freq change
    if (omsynth[clk] != outdivider || o_rdiv[clk] != r) {

        // CLK# registers are exactly 8 * clk# bytes stride from a base register.
        msyn_stride = clk * 8;

        // keep track of the change
        omsynth[clk] = (uint16_t)outdivider;
        o_rdiv[clk] = r; // cache it now, before we OR mask up R for special divide by 4

        // See datasheet, special trick when MSx == 4
        //    MSx_P1 is always 0 if outdivider == 4, from the above equations, so
        //    there is no need to set it to 0. ... MSx_P1 = 128 * outdivider - 512;
        //      See para 4.1.3 on the datasheet.
        if (outdivider == 4) {
            r |= 0x0C; // bit set OR mask for MSYNTH divide by 4, for reg 44 {3:2]
        }

        uint8_t reg_bank_42[] = {
            0,                                // bits [15:8]  of MS0_P3 (always 0) in register 42
            1,                                // bits [7:0]   of MS0_P3 (always 1) in register 43
            ((msx_p1 & 0x030000L) >> 16) | r, // bits [17:16] of MSx_P1 in bits [1:0] and R in [7:4] | [3:2]
            (msx_p1 & 0xFF00) >> 8,           // bits [15:8]  of MSx_P1 in register 45
            msx_p1 & 0xFF,                    // bits [7:0]   of MSx_P1 in register 46
            0,                                // bits [19:16] of MS0_P2 and MS0_P3 are always 0
            0,                                // bits [15:8]  of MS0_P2 are always 0
            0                                 // bits [7:0]   of MS0_P2 are always 0
        };

        // Get the two write bursts as close together as possible, to attempt to reduce any more click glitches. This is
        // at the expense of only 24 increased bytes compilation size in AVR 328. Everything is already precalculated
        // above, reducing any delay, by not doing calculations between the burst writes.
        si5351_i2c_write_bulk(si5351_dev, 26 + pll_stride, sizeof(reg_bank_26), reg_bank_26);
        si5351_i2c_write_bulk(si5351_dev, 42 + msyn_stride, sizeof(reg_bank_42), reg_bank_42);

        //
        // https://www.silabs.com/documents/public/application-notes/Si5350-Si5351%20FAQ.pdf
        //
        // 11.1 "The Int, R and N register values inside the Interpolative Dividers are updated when the LSB of R is
        // written via I2C."
        // - Q. does this mean reg 44 or 49 (misprint ?) ???
        //
        // 10.1 "All outputs are within +/- 500ps of one another at power up (if pre-programmed) or if PLLA and PLLB are
        // reset simultaneously via register 177."
        //
        // 17.1 "The PLL can track any abrupt input frequency changes of 3–4% without losing lock to it. Any input f
        // equency changes greater than this amount
        //      will not necessarily track from the input to the output must reset the so called "PLL", in fact the
        //      output msynth
        si5351_fast_reset(si5351_dev);

    } else {
        si5351_i2c_write_bulk(si5351_dev, 26 + pll_stride, sizeof(reg_bank_26), reg_bank_26);
    }
}

void
si5351_calc(si5351_t *si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num, int32_t *pll_denom,
            int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv,
            uint8_t *out_allow_integer_mode) {
    // Here we are looking for integer values of a,b,c,x,y,z such as:
    // N = a + b / c    # pll settings
    // M = x + y / z    # ms  settings
    // Fclk = Fxtal * N / M
    // N in [24, 36]
    // M in [8, 1800] or M in {4,6}
    // b < c, y < z
    // b,c,y,z <= 2**20
    // c, z != 0
    // For any Fclk in [500K, 160MHz] this algorithm finds a solution
    // such as abs(Ffound - Fclk) <= 6 Hz
    int32_t a, b, c, x, y, z, t;
    int32_t numerator;
    int32_t fpll;

    if (fclk < 8000)
        fclk = 8000;
    else if (fclk > 160000000)
        fclk = 160000000;

    *out_allow_integer_mode = 1;

    if (fclk < 1000000) {
        // For frequencies in [8_000, 500_000] range we can use si5351_Calc(Fclk*64, ...) and SI5351_R_DIV_64. In
        // practice it's worth doing for any frequency below 1 MHz, since it reduces the error.
        fclk *= 64;
        *out_rdiv = SI5351_R_DIV_64;
    } else {
        *out_rdiv = SI5351_R_DIV_1;
    }

    // Apply correction, _after_ determining rdiv.
    fclk = fclk - (int32_t)((((double)fclk) / 100000000.0) * ((double)corr));

    if (fclk < 81000000) {
        // Valid for Fclk in 0.5..112.5 MHz range. However an error is > 6 Hz above 81 MHz
        a = 36; // PLL runs @ 900 MHz
        b = 0;
        c = 1;
        fpll = 900000000;
        x = fpll / fclk;
        t = (fclk >> 20) + 1;
        y = (fpll % fclk) / t;
        z = fclk / t;
    } else {
        // Valid for fclk in 75..160 MHz range
        if (fclk >= 150000000) {
            x = 4;
        } else if (fclk >= 100000000) {
            x = 6;
        } else {
            x = 8;
        }
        y = 0;
        z = 1;

        numerator = x * fclk;
        a = numerator / SI5351_XTAL_FREQ;
        t = (SI5351_XTAL_FREQ >> 20) + 1;
        b = (numerator % SI5351_XTAL_FREQ) / t;
        c = SI5351_XTAL_FREQ / t;
    }

    *pll_mult = a;
    *pll_num = b;
    *pll_denom = c;
    *out_div = x;
    *out_num = y;
    *out_denom = z;
}

void
si5351_calc_iq(si5351_t *si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num,
               int32_t *pll_denom, int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv,
               uint8_t *out_allow_integer_mode) {
    int32_t fpll;

    if (fclk < 1400000)
        fclk = 1400000;
    else if (fclk > 100000000)
        fclk = 100000000;

    // apply correction
    fclk = fclk - ((fclk / 1000000) * corr) / 100;

    // disable integer mode
    *out_allow_integer_mode = 0;

    // Using RDivider's changes the phase shift and AN619 doesn't give any guarantees regarding this change.
    *out_rdiv = 0;

    if (fclk < 4900000) {
        // Little hack, run PLL below 600 MHz to cover 1.4 MHz .. 4.725 MHz range.
        // AN619 doesn't literally say that PLL can't run below 600 MHz.
        // Experiments showed that PLL gets unstable when you run it below 177 MHz, which limits Fclk to 177 / 127 = 1.4
        // MHz.
        *out_div = 127;
    } else if (fclk < 8000000) {
        *out_div = 625000000 / fclk;
    } else {
        *out_div = 900000000 / fclk;
    }
    *out_num = 0;
    *out_denom = 1;

    fpll = fclk * (*out_div);
    *pll_mult = fpll / SI5351_XTAL_FREQ;
    *pll_num = (fpll % SI5351_XTAL_FREQ) / 24;
    *pll_denom = SI5351_XTAL_FREQ / 24; // denom can't exceed 0xFFFFF
}
