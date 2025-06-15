/*
 * Copyright 2024 Emiliano Gonzalez (egonzalez . hiperion @ gmail . com))
 * * Project Site: https://github.com/hiperiondev/esp32-si5351 *
 *
 * This is based on other projects:
 *    Si5351 library for Arduino: Jason Milldrum <milldrum@gmail.com>, Dana H. Myers <k6jq@comcast.net>
 *    HAL-based Si5351 driver for STM32 : https://github.com/afiskon/stm32-si5351
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

#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"

#define SI5351_I2C_TIMEOUT_MS 1000
#define SI5351_BUS_BASE_ADDR 0x60
#define SI5351_XTAL_FREQ 25000000
#define SI5351_PLL_FIXED 80000000000ULL
#define SI5351_FREQ_MULT 100ULL
#define SI5351_DEFAULT_CLK 1000000000ULL
#define SI5351_PLL_VCO_MIN 600000000
#define SI5351_PLL_VCO_MAX 900000000
#define SI5351_MULTISYNTH_MIN_FREQ 500000
#define SI5351_MULTISYNTH_DIVBY4_FREQ 150000000
#define SI5351_MULTISYNTH_MAX_FREQ 225000000
#define SI5351_MULTISYNTH_SHARE_MAX 100000000
#define SI5351_MULTISYNTH_SHARE_MIN 1024000
#define SI5351_MULTISYNTH67_MAX_FREQ SI5351_MULTISYNTH_DIVBY4_FREQ
#define SI5351_CLKOUT_MIN_FREQ 4000
#define SI5351_CLKOUT_MAX_FREQ SI5351_MULTISYNTH_MAX_FREQ
#define SI5351_CLKOUT67_MS_MIN SI5351_PLL_VCO_MIN / SI5351_MULTISYNTH67_A_MAX
#define SI5351_CLKOUT67_MIN_FREQ SI5351_CLKOUT67_MS_MIN / 128
#define SI5351_CLKOUT67_MAX_FREQ SI5351_MULTISYNTH67_MAX_FREQ
#define SI5351_PLL_A_MIN 15
#define SI5351_PLL_A_MAX 90
#define SI5351_PLL_B_MAX (SI5351_PLL_C_MAX - 1)
#define SI5351_PLL_C_MAX 1048575
#define SI5351_MULTISYNTH_A_MIN 6
#define SI5351_MULTISYNTH_A_MAX 1800
#define SI5351_MULTISYNTH67_A_MAX 254
#define SI5351_MULTISYNTH_B_MAX (SI5351_MULTISYNTH_C_MAX - 1)
#define SI5351_MULTISYNTH_C_MAX 1048575
#define SI5351_MULTISYNTH_P1_MAX ((1 << 18) - 1)
#define SI5351_MULTISYNTH_P2_MAX ((1 << 20) - 1)
#define SI5351_MULTISYNTH_P3_MAX ((1 << 20) - 1)
#define SI5351_VCXO_PULL_MIN 30
#define SI5351_VCXO_PULL_MAX 240
#define SI5351_VCXO_MARGIN 103
#define SI5351_DEVICE_STATUS 0
#define SI5351_INTERRUPT_STATUS 1
#define SI5351_INTERRUPT_MASK 2
#define SI5351_STATUS_SYS_INIT (1 << 7)
#define SI5351_STATUS_LOL_B (1 << 6)
#define SI5351_STATUS_LOL_A (1 << 5)
#define SI5351_STATUS_LOS (1 << 4)
#define SI5351_OUTPUT_ENABLE_CTRL 3
#define SI5351_OEB_PIN_ENABLE_CTRL 9
#define SI5351_PLL_INPUT_SOURCE 15
#define SI5351_CLKIN_DIV_MASK (3 << 6)
#define SI5351_CLKIN_DIV_1 (0 << 6)
#define SI5351_CLKIN_DIV_2 (1 << 6)
#define SI5351_CLKIN_DIV_4 (2 << 6)
#define SI5351_CLKIN_DIV_8 (3 << 6)
#define SI5351_PLLB_SOURCE (1 << 3)
#define SI5351_PLLA_SOURCE (1 << 2)
#define SI5351_CLK0_CTRL 16
#define SI5351_CLK1_CTRL 17
#define SI5351_CLK2_CTRL 18
#define SI5351_CLK3_CTRL 19
#define SI5351_CLK4_CTRL 20
#define SI5351_CLK5_CTRL 21
#define SI5351_CLK6_CTRL 22
#define SI5351_CLK7_CTRL 23
#define SI5351_CLK_POWERDOWN (1 << 7)
#define SI5351_CLK_INTEGER_MODE (1 << 6)
#define SI5351_CLK_PLL_SELECT (1 << 5)
#define SI5351_CLK_INVERT (1 << 4)
#define SI5351_CLK_INPUT_MASK (3 << 2)
#define SI5351_CLK_INPUT_XTAL (0 << 2)
#define SI5351_CLK_INPUT_CLKIN (1 << 2)
#define SI5351_CLK_INPUT_MULTISYNTH_0_4 (2 << 2)
#define SI5351_CLK_INPUT_MULTISYNTH_N (3 << 2)
#define SI5351_CLK_DRIVE_STRENGTH_MASK (3 << 0)
#define SI5351_CLK_DRIVE_STRENGTH_2MA (0 << 0)
#define SI5351_CLK_DRIVE_STRENGTH_4MA (1 << 0)
#define SI5351_CLK_DRIVE_STRENGTH_6MA (2 << 0)
#define SI5351_CLK_DRIVE_STRENGTH_8MA (3 << 0)
#define SI5351_CLK3_0_DISABLE_STATE 24
#define SI5351_CLK7_4_DISABLE_STATE 25
#define SI5351_CLK_DISABLE_STATE_MASK 3
#define SI5351_CLK_DISABLE_STATE_LOW 0
#define SI5351_CLK_DISABLE_STATE_HIGH 1
#define SI5351_CLK_DISABLE_STATE_FLOAT 2
#define SI5351_CLK_DISABLE_STATE_NEVER 3
#define SI5351_PARAMETERS_LENGTH 8
#define SI5351_PLLA_PARAMETERS 26
#define SI5351_PLLB_PARAMETERS 34
#define SI5351_CLK0_PARAMETERS 42
#define SI5351_CLK1_PARAMETERS 50
#define SI5351_CLK2_PARAMETERS 58
#define SI5351_CLK3_PARAMETERS 66
#define SI5351_CLK4_PARAMETERS 74
#define SI5351_CLK5_PARAMETERS 82
#define SI5351_CLK6_PARAMETERS 90
#define SI5351_CLK7_PARAMETERS 91
#define SI5351_CLK6_7_OUTPUT_DIVIDER 92
#define SI5351_OUTPUT_CLK_DIV_MASK (7 << 4)
#define SI5351_OUTPUT_CLK6_DIV_MASK (7 << 0)
#define SI5351_OUTPUT_CLK_DIV_SHIFT 4
#define SI5351_OUTPUT_CLK_DIV6_SHIFT 0
#define SI5351_OUTPUT_CLK_DIV_1 0
#define SI5351_OUTPUT_CLK_DIV_2 1
#define SI5351_OUTPUT_CLK_DIV_4 2
#define SI5351_OUTPUT_CLK_DIV_8 3
#define SI5351_OUTPUT_CLK_DIV_16 4
#define SI5351_OUTPUT_CLK_DIV_32 5
#define SI5351_OUTPUT_CLK_DIV_64 6
#define SI5351_OUTPUT_CLK_DIV_128 7
#define SI5351_OUTPUT_CLK_DIVBY4 (3 << 2)
#define SI5351_SSC_PARAM0 149 // spread spectrum
#define SI5351_SSC_PARAM1 150
#define SI5351_SSC_PARAM2 151
#define SI5351_SSC_PARAM3 152
#define SI5351_SSC_PARAM4 153
#define SI5351_SSC_PARAM5 154
#define SI5351_SSC_PARAM6 155
#define SI5351_SSC_PARAM7 156
#define SI5351_SSC_PARAM8 157
#define SI5351_SSC_PARAM9 158
#define SI5351_SSC_PARAM10 159
#define SI5351_SSC_PARAM11 160
#define SI5351_SSC_PARAM12 161
#define SI5351_VXCO_PARAMETERS_LOW 162
#define SI5351_VXCO_PARAMETERS_MID 163
#define SI5351_VXCO_PARAMETERS_HIGH 164
#define SI5351_CLK0_PHASE_OFFSET 165
#define SI5351_CLK1_PHASE_OFFSET 166
#define SI5351_CLK2_PHASE_OFFSET 167
#define SI5351_CLK3_PHASE_OFFSET 168
#define SI5351_CLK4_PHASE_OFFSET 169
#define SI5351_CLK5_PHASE_OFFSET 170
#define SI5351_PLL_RESET 177
#define SI5351_PLL_RESET_B (1 << 7)
#define SI5351_PLL_RESET_A (1 << 5)
#define SI5351_CRYSTAL_LOAD 183
#define SI5351_CRYSTAL_LOAD_MASK (3 << 6)
#define SI5351_CRYSTAL_LOAD_0PF (0 << 6)
#define SI5351_CRYSTAL_LOAD_6PF (1 << 6)
#define SI5351_CRYSTAL_LOAD_8PF (2 << 6)
#define SI5351_CRYSTAL_LOAD_10PF (3 << 6)
#define SI5351_FANOUT_ENABLE 187
#define SI5351_CLKIN_ENABLE (1 << 7)
#define SI5351_XTAL_ENABLE (1 << 6)
#define SI5351_MULTISYNTH_ENABLE (1 << 4)
#define RFRAC_DENOM 1000000ULL
// #define RFRAC_DENOM ((1L << 20) - 1)

// Based on former asm-ppc/div64.h and asm-m68knommu/div64.h
// The semantics of do_div() are:
//
// uint32_t do_div(uint64_t *n, uint32_t base)
// {
//      uint32_t remainder = *n % base;
//      *n = *n / base;
//      return remainder;
// }
// NOTE: macro parameter n is evaluated multiple times,
//       beware of side effects!
#define do_div(n, base)                                                                                                \
    ({                                                                                                                 \
        uint64_t __base = (base);                                                                                      \
        uint64_t __rem;                                                                                                \
        __rem = ((uint64_t)(n)) % __base;                                                                              \
        (n) = ((uint64_t)(n)) / __base;                                                                                \
        __rem;                                                                                                         \
    })

/**
 * @enum si5351_variant
 * @brief SiLabs Si5351 chip variant
 *
 */
enum si5351_variant {
    SI5351_VARIANT_A = 1,  /**< Si5351A (8 output clocks, XTAL input)        */
    SI5351_VARIANT_A3 = 2, /**< Si5351A MSOP10 (3 output clocks, XTAL input) */
    SI5351_VARIANT_B = 3,  /**< Si5351B (8 output clocks, XTAL/VXCO input)   */
    SI5351_VARIANT_C = 4,  /**< Si5351C (8 output clocks, XTAL/CLKIN input)  */
};

/**
 * @enum si5351_rdiv
 * @brief
 *
 */
enum si5351_rdiv {
    SI5351_R_DIV_1 = 0,   /**< SI5351_R_DIV_1   */
    SI5351_R_DIV_2 = 1,   /**< SI5351_R_DIV_2   */
    SI5351_R_DIV_4 = 2,   /**< SI5351_R_DIV_4   */
    SI5351_R_DIV_8 = 3,   /**< SI5351_R_DIV_8   */
    SI5351_R_DIV_16 = 4,  /**< SI5351_R_DIV_16  */
    SI5351_R_DIV_32 = 5,  /**< SI5351_R_DIV_32  */
    SI5351_R_DIV_64 = 6,  /**< SI5351_R_DIV_64  */
    SI5351_R_DIV_128 = 7, /**< SI5351_R_DIV_128 */
};

/**
 * @enum si5351_clock
 * @brief
 *
 */
enum si5351_clock {
    SI5351_CLK0, /**< SI5351_CLK0 */
    SI5351_CLK1, /**< SI5351_CLK1 */
    SI5351_CLK2, /**< SI5351_CLK2 */
    SI5351_CLK3, /**< SI5351_CLK3 */
    SI5351_CLK4, /**< SI5351_CLK4 */
    SI5351_CLK5, /**< SI5351_CLK5 */
    SI5351_CLK6, /**< SI5351_CLK6 */
    SI5351_CLK7, /**< SI5351_CLK7 */
};

/**
 * @enum si5351_pll
 * @brief
 *
 */
enum si5351_pll {
    SI5351_PLLA, /**< SI5351_PLLA */
    SI5351_PLLB  /**< SI5351_PLLB */
};

/**
 * @enum si5351_drive
 * @brief
 *
 */
enum si5351_drive {
    SI5351_DRIVE_2MA, /**< SI5351_DRIVE_2MA */
    SI5351_DRIVE_4MA, /**< SI5351_DRIVE_4MA */
    SI5351_DRIVE_6MA, /**< SI5351_DRIVE_6MA */
    SI5351_DRIVE_8MA, /**< SI5351_DRIVE_8MA */
};

/**
 * @enum si5351_clock_source
 * @brief
 *
 */
enum si5351_clock_source {
    SI5351_CLK_SRC_XTAL,  /**< SI5351_CLK_SRC_XTAL  */
    SI5351_CLK_SRC_CLKIN, /**< SI5351_CLK_SRC_CLKIN */
    SI5351_CLK_SRC_MS0,   /**< SI5351_CLK_SRC_MS0   */
    SI5351_CLK_SRC_MS,    /**< SI5351_CLK_SRC_MS    */
};

/**
 * @enum si5351_clock_disable
 * @brief
 *
 */
enum si5351_clock_disable {
    SI5351_CLK_DISABLE_LOW,   /**< SI5351_CLK_DISABLE_LOW   */
    SI5351_CLK_DISABLE_HIGH,  /**< SI5351_CLK_DISABLE_HIGH  */
    SI5351_CLK_DISABLE_HI_Z,  /**< SI5351_CLK_DISABLE_HI_Z  */
    SI5351_CLK_DISABLE_NEVER, /**< SI5351_CLK_DISABLE_NEVER */
};

/**
 * @enum si5351_clock_fanout
 * @brief
 *
 */
enum si5351_clock_fanout {
    SI5351_FANOUT_CLKIN, /**< SI5351_FANOUT_CLKIN */
    SI5351_FANOUT_XO,    /**< SI5351_FANOUT_XO    */
    SI5351_FANOUT_MS,    /**< SI5351_FANOUT_MS    */
};

/**
 * @enum si5351_pll_input
 * @brief
 *
 */
enum si5351_pll_input {
    SI5351_PLL_INPUT_XO,   /**< SI5351_PLL_INPUT_XO    */
    SI5351_PLL_INPUT_CLKIN /**< SI5351_PLL_INPUT_CLKIN */
};

/**
 * @struct si5351_reg_set
 * @brief
 *
 */
struct si5351_reg_set {
    uint32_t p1; /**< p1 */
    uint32_t p2; /**< p2 */
    uint32_t p3; /**< p3 */
};

/**
 * @struct si5351_status
 * @brief
 *
 */
struct si5351_status {
    uint8_t SYS_INIT; /**< SYS_INIT */
    uint8_t LOL_B;    /**< LOL_B    */
    uint8_t LOL_A;    /**< LOL_A    */
    uint8_t LOS;      /**< LOS      */
    uint8_t REVID;    /**< REVID    */
};

/**
 * @struct si5351_int_status
 * @brief
 *
 */
struct si5351_int_status {
    uint8_t SYS_INIT_STKY; /**< SYS_INIT_STKY */
    uint8_t LOL_B_STKY;    /**< LOL_B_STKY    */
    uint8_t LOL_A_STKY;    /**< LOL_A_STKY    */
    uint8_t LOS_STKY;      /**< LOS_STKY      */
};

/*
 * @struct si5351_i2c_s
 * @brief
 *
 */
typedef struct si5351_i2c_s {
    i2c_master_bus_config_t i2c_bus_config;
    i2c_device_config_t i2c_dev_conf;
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle;
} si5351_i2c_t;

/*
 * @struct si5351_s
 * @brief
 *
 */
typedef struct si5351_s {
    si5351_i2c_t i2c_dev;                           /**< i2c device */
    struct si5351_status si5351_dev_status;         /**< si5351_dev_status */
    struct si5351_int_status si5351_dev_int_status; /**< si5351_dev_int_status */
    enum si5351_pll pll_assignment[8];              /**< pll_assignment */
    uint64_t si5351_clk_freq[8];                    /**< si5351_clk_freq */
    uint64_t si5351_plla_freq;                      /**< si5351_plla_freq */
    uint64_t si5351_pllb_freq;                      /**< si5351_pllb_freq */
    enum si5351_pll_input plla_ref_osc;             /**< plla_ref_osc */
    enum si5351_pll_input pllb_ref_osc;             /**< pllb_ref_osc */
    uint32_t si5351_xtal_freq[2];                   /**< si5351_xtal_freq */
    int32_t si5351_ref_correction[2];               /**< si5351_ref_correction */
    uint8_t si5351_clkin_div;                       /**< si5351_clkin_div */
    bool si5351_clk_first_set[8];                   /**< si5351_clk_first_set */
} si5351_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int8_t si5351_i2c_init(si5351_t *si5351_dev, i2c_port_num_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio,
                       uint32_t clk_speed);

/**
 * @fn esp_err_t si5351_init(si5351_t* si5351_dev, uint8_t xtal_load_c, uint32_t xo_freq, int32_t corr, i2c_port_t port,
 * gpio_num_t sda_gpio, gpio_num_t scl_gpio, uint32_t clk_speed);
 * @brief Setup communications to the Si5351 and set the crystal load capacitance.
 *
 * @param si5351_dev Device
 * @param xtal_load_c Crystal load capacitance. Use the SI5351_CRYSTAL_LOAD_*PF defines in the header file
 * @param xo_freq Crystal/reference oscillator frequency in 1 Hz increments. Defaults to 25000000 if a 0 is used here.
 * @param corr Frequency correction constant in parts-per-billion
 * @return boolean that indicates whether a device was found on the desired I2C address.
 */
esp_err_t si5351_init(si5351_t *si5351_dev, uint8_t xtal_load_c, uint32_t xo_freq, int32_t corr);

/**
 * @fn void si5351_reset(si5351_t* si5351_dev)
 * @brief Call to reset the Si5351 to the state initialized by the library.
 *
 */
void si5351_reset(si5351_t *si5351_dev);

/**
 * @fn void si5351_fast_reset(si5351_t* si5351_dev)
 * @brief Reset of the PLLs and multisynths output enable

 * This must be called to soft reset the PLLs and cycle the output of the multisynths: this is the "click" noise source
 in the RF spectrum.
 * So it must be avoided at all costs, so this lib just call it at the initialization of the PLLs and when a correction
 is applied.
 * If you are concerned with accuracy you can implement a reset every other Mhz to be sure it get exactly on spot.
 *
 * @param si5351_dev Device
 */
void si5351_fast_reset(si5351_t *si5351_dev);

/**
 * @fn bool si5351_set_freq(si5351_t* si5351_dev, uint64_t freq, enum si5351_clock clk)
 * @brief Sets the clock frequency of the specified CLK output. Frequency range of 8 kHz to 150 MHz
 *
 * @param si5351_dev Device
 * @param freq Output frequency in Hz
 * @param clk Clock output (use the si5351_clock enum)
 * @return true: ok
 */
bool si5351_set_freq(si5351_t *si5351_dev, uint64_t freq, enum si5351_clock clk);

/**
 * @fn void si5351_set_freq2(si5351_t* si5351_dev, uint8_t clk, uint32_t freq)
 * @brief This function set the freq of the corresponding clock.
 * (from https://github.com/pavelmc/Si5351mcu)
 * In tests on Si5351 can work between 7,8 Khz and ~225 Mhz [~250 MHz with overclocking] as usual YMMV Click noise:
 * - The lib has a reset programmed [aka: click noise] every time it needs to change the output divider of a particular
 * MSynth, if you move in big steps this can lead to an increased rate of click noise per tunning step.
 * - If you move at a pace of a few Hz each time the output divider will change at a low rate, hence less click noise
 * per tunning step.
 * - The output divider moves [change] faster at high frequencies, so at HF the clikc noise is at the real minimum
 * possible.
 *
 * @param si5351_dev Device
 * @param freq Output frequency in Hz
 * @param clk Clock output (use the si5351_clock enum)
 */
void si5351_set_freq2(si5351_t *si5351_dev, uint64_t freq, enum si5351_clock clk);

/**
 * @fn bool si5351_set_freq_manual(si5351_t* si5351_dev, uint64_t freq, uint64_t pll_freq, enum si5351_clock clk)
 * @brief Sets the clock frequency of the specified CLK output using the given PLL frequency. You must ensure that the
 * MS is assigned to the correct PLL and that the PLL is set to the correct frequency before using this method. It is
 * important to note that if you use this method, you will have to track that all settings are sane yourself.
 *
 * @param si5351_dev Device
 * @param freq Output frequency in Hz
 * @param pll_freq Frequency of the PLL driving the Multisynth in Hz * 100
 * @param clk Clock output (use the si5351_clock enum)
 * @return true: ok
 */
bool si5351_set_freq_manual(si5351_t *si5351_dev, uint64_t freq, uint64_t pll_freq, enum si5351_clock clk);

/**
 * @fn void si5351_set_pll(si5351_t* si5351_dev, uint64_t pll_freq, enum si5351_pll target_pll)
 * @brief Set the specified PLL to a specific oscillation frequency.
 *
 * @param si5351_dev Device
 * @param pll_freq Desired PLL frequency in Hz * 100
 * @param target_pll Which PLL to set (use the si5351_pll enum)
 */
void si5351_set_pll(si5351_t *si5351_dev, uint64_t pll_freq, enum si5351_pll target_pll);

/**
 * @fn void si5351_set_ms(si5351_t* si5351_dev, enum si5351_clock clk, struct si5351_reg_set ms_reg, uint8_t int_mode,
 * uint8_t r_div, uint8_t div_by_4)
 * @brief Set the specified multisynth parameters. Not normally needed, but public for advanced users.
 *
 * @param si5351_dev Device
 * @param clk  Clock output (use the si5351_clock enum)
 * @param ms_reg
 * @param int_mode Set integer mode. Set to 1 to enable, 0 to disable
 * @param r_div Desired r_div ratio
 * @param div_by_4 Set Divide By 4 mode. Set to 1 to enable, 0 to disable
 */
void si5351_set_ms(si5351_t *si5351_dev, enum si5351_clock clk, struct si5351_reg_set ms_reg, uint8_t int_mode,
                   uint8_t r_div, uint8_t div_by_4);

/**
 * @fn void si5351_output_enable(si5351_t* si5351_dev, enum si5351_clock clk, uint8_t enable)
 * @brief Enable or disable a chosen output
 *
 * @param si5351_dev Device
 * @param clk Clock output. (use the si5351_clock enum)
 * @param enable Set to true to enable, false to disable
 */
void si5351_output_enable(si5351_t *si5351_dev, enum si5351_clock clk, bool enable);

/**
 * @fn void si5351_drive_strength(si5351_t* si5351_dev, enum si5351_clock clk, enum si5351_drive drive)
 * @brief Sets the drive strength of the specified clock output
 *
 * @param si5351_dev Device
 * @param clk Clock output. (use the si5351_clock enum)
 * @param drive Desired drive level. (use the si5351_drive enum)
 */
void si5351_drive_strength(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_drive drive);

/**
 * @fn void si5351_update_status(si5351_t* si5351_dev)
 * @brief Call this to update the status structs, then access them via the dev_status and dev_int_status global members.
 * See the header file for the struct definitions. These correspond to the flag names for registers 0 and 1 in the
 * Si5351 datasheet.
 *
 * @param si5351_dev Device
 */
void si5351_update_status(si5351_t *si5351_dev);

/**
 * @fn void si5351_set_correction(si5351_t* si5351_dev, int32_t corr, enum si5351_pll_input ref_osc)
 * @brief  Use this to set the oscillator correction factor. This value is a signed 32-bit integer of the
 * parts-per-billion value that the actual oscillation frequency deviates from the specified frequency.
 *
 * The frequency calibration is done as a one-time procedure. Any desired test frequency within the normal range of the
 * Si5351 should be set, then the actual output frequency should be measured as accurately as possible. The difference
 * between the measured and specified frequencies should be calculated in Hertz, then multiplied by 10 in order to get
 * the parts-per-billion value. Since the Si5351 itself has an intrinsic 0 PPM error, this correction factor is good
 * across the entire tuning range of the Si5351. Once this calibration is done accurately, it should not have to be done
 * again for the same Si5351 and crystal.
 *
 * @param si5351_dev Device
 * @param corr Correction factor in ppb
 * @param ref_osc Desired reference oscillator (use the si5351_pll_input enum)
 */
void si5351_set_correction(si5351_t *si5351_dev, int32_t corr, enum si5351_pll_input ref_osc);

/**
 * @fn void si5351_set_phase(si5351_t* si5351_dev, enum si5351_clock clk, uint8_t phase)
 * @brief Write the 7-bit phase register. This must be used with a user-set PLL frequency so that the user can calculate
 * the proper tuning word based on the PLL period.
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param phase 7-bit phase word (in units of VCO/4 period)
 */
void si5351_set_phase(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t phase);

/**
 * @fn int32_t si5351_get_correction(si5351_t* si5351_dev, enum si5351_pll_input ref_osc)
 * @brief Returns the oscillator correction factor stored in RAM.
 *
 * @param si5351_dev Device
 * @param ref_osc Desired reference oscillator 0: crystal oscillator (XO), 1: external clock input (CLKIN)
 * @return Oscillator correction factor stored in RAM.
 */
int32_t si5351_get_correction(si5351_t *si5351_dev, enum si5351_pll_input ref_osc);

/**
 * @fn void si5351_pll_reset(si5351_t* si5351_dev, enum si5351_pll target_pll)
 * @brief Apply a reset to the indicated PLL.
 *
 * @param si5351_dev Device
 * @param target_pll Which PLL to reset (use the si5351_pll enum)
 */
void si5351_pll_reset(si5351_t *si5351_dev, enum si5351_pll target_pll);

/**
 * @fn void si5351_set_ms_source(si5351_t* si5351_dev, enum si5351_clock clk, enum si5351_pll pll)
 * @brief Set the desired PLL source for a multisynth.
 *
 * @param si5351_dev Device
 * @param clk Clock output use the si5351_clock enum)
 * @param pll Which PLL to use as the source (use the si5351_pll enum)
 */
void si5351_set_ms_source(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_pll pll);

/**
 * @fn void si5351_set_int(si5351_t* si5351_dev, enum si5351_clock clk, uint8_t enable)
 * @brief Set the indicated multisynth into integer mode.
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param enable Set to 1 to enable, 0 to disable
 */
void si5351_set_int(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t enable);

/**
 * @fn void si5351_set_clock_pwr(si5351_t* si5351_dev, enum si5351_clock clk, uint8_t pwr)
 * @brief Enable or disable power to a clock output (a power saving feature).
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param pwr Set to 1 to enable, 0 to disable
 */
void si5351_set_clock_pwr(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t pwr);

/**
 * @fn void si5351_set_clock_invert(si5351_t* si5351_dev, enum si5351_clock clk, uint8_t inv)
 * @brief Enable to invert the clock output waveform.
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param inv Set to 1 to enable, 0 to disable
 */
void si5351_set_clock_invert(si5351_t *si5351_dev, enum si5351_clock clk, uint8_t inv);

/**
 * @fn void si5351_set_clock_source(si5351_t* si5351_dev, enum si5351_clock clk, enum
 * si5351_clock_source src)
 * @brief Set the clock source for a multisynth (based on the options presented for Registers 16-23 in the Silicon Labs
 * AN619 document). Choices are XTAL, CLKIN, MS0, or the multisynth associated with the clock output.
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param src Which clock source to use for the multisynth (use the si5351_clock_source enum)
 */
void si5351_set_clock_source(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_clock_source src);

/**
 * @fn void si5351_set_clock_disable(si5351_t* si5351_dev, enum si5351_clock clk, enum si5351_clock_disable dis_state)
 * @brief Set the state of the clock output when it is disabled. Per page 27 of AN619 (Registers 24 and 25), there are
 * four possible values: low, high, high impedance, and never disabled
 *
 * @param si5351_dev Device
 * @param clk Clock output (use the si5351_clock enum)
 * @param dis_state Desired state of the output upon disable (use the si5351_clock_disable enum)
 */
void si5351_set_clock_disable(si5351_t *si5351_dev, enum si5351_clock clk, enum si5351_clock_disable dis_state);

/**
 * @fn void si5351_set_clock_fanout(si5351_t* si5351_dev, si5351_t* si5351_dev, enum si5351_clock_fanout fanout, uint8_t
 * enable)
 * @brief Use this function to enable or disable the clock fanout options for individual clock outputs. If you intend to
 * output the XO or CLKIN on the clock outputs, enable this first.
 *
 * @param si5351_dev Device
 * @param fanout Desired clock fanout (use the si5351_clock_fanout enum)
 * @param enable Set to 1 to enable, 0 to disable
 */
void si5351_set_clock_fanout(si5351_t *si5351_dev, enum si5351_clock_fanout fanout, uint8_t enable);

/**
 * @fn void si5351_set_pll_input(si5351_t* si5351_dev, enum si5351_pll pll, enum si5351_pll_input input)
 * @brief Set the desired reference oscillator source for the given PLL.
 *
 * @param si5351_dev Device
 * @param pll Which PLL to use as the source (use the si5351_pll enum)
 * @param input Which reference oscillator to use as PLL input (use the si5351_pll_input enum)
 */
void si5351_set_pll_input(si5351_t *si5351_dev, enum si5351_pll pll, enum si5351_pll_input input);

/**
 * @fn void si5351_set_vcxo(si5351_t* si5351_dev, uint64_t pll_freq, uint8_t ppm)
 * @brief Set the parameters for the VCXO on the Si5351B.
 *
 * @param si5351_dev Device
 * @param pll_freq Desired PLL base frequency in Hz * 100
 * @param ppm VCXO pull limit in ppm
 */
void si5351_set_vcxo(si5351_t *si5351_dev, uint64_t pll_freq, uint8_t ppm);

/**
 * @fn void si5351_set_ref_freq(si5351_t* si5351_dev, uint32_t ref_freq, enum si5351_pll_input ref_osc)
 * @brief Set the reference frequency value for the desired reference oscillator
 *
 * @param si5351_dev Device
 * @param ref_freq Reference oscillator frequency in Hz
 * @param ref_osc Which reference oscillator frequency to set (use the si5351_pll_input enum)
 */
void si5351_set_ref_freq(si5351_t *si5351_dev, uint32_t ref_freq, enum si5351_pll_input ref_osc);

/**
 * @fn void si5351_spread_spectrum(si5351_t* si5351_dev, bool enabled)
 * @brief Enables or disables spread spectrum
 *
 * @param si5351_dev Device
 * @param enabled Whether spread spectrum output is enabled
 */
void si5351_spread_spectrum(si5351_t *si5351_dev, bool enabled);

/**
 * @fn void si5351_calc(si5351_t* si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num, int32_t
 * *pll_denom, int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv, uint8_t
 * *out_allow_integer_mode);
 * @brief Calculates PLL, MS and RDiv settings for given fclk in [8_000, 160_000_000] range. The actual frequency will
 * differ less than 6 Hz from given fclk, assuming `correction` is right.
 *
 * @param si5351_dev Device
 * @param fclk
 * @param corr
 * @param pll_mult
 * @param pll_num
 * @param pll_denom
 * @param out_div
 * @param out_num
 * @param out_denom
 * @param out_rdiv
 * @param out_allow_integer_mode
 */
void si5351_calc(si5351_t *si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num,
                 int32_t *pll_denom, int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv,
                 uint8_t *out_allowIntegerMode);

/**
 * @fn void si5351_calc_iq(si5351_t* si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num,
 * int32_t *pll_denom, int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv, uint8_t
 * *out_allow_integer_mode);
 * @brief Finds PLL and MS parameters that give phase shift 90° between two channels. If 0 and (uint8_t)out_div are
 * assed as phaseOffset for these channels. Channels should use the same PLL to make it work. fclk can be from 1.4 MHz
 * to 100 MHz. The actual frequency will differ less than 4 Hz from given fclk, assuming `correction` is right
 *
 * @param si5351_dev Device
 * @param fclk
 * @param corr
 * @param pll_mult
 * @param pll_num
 * @param pll_denom
 * @param out_div
 * @param out_num
 * @param out_denom
 * @param out_rdiv
 * @param out_allow_integer_mode
 */
void si5351_calc_iq(si5351_t *si5351_dev, int32_t fclk, int32_t corr, int32_t *pll_mult, int32_t *pll_num,
                    int32_t *pll_denom, int32_t *out_div, int32_t *out_num, int32_t *out_denom, uint8_t *out_rdiv,
                    uint8_t *out_allow_integer_mode);
