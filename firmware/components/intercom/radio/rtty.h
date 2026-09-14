#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const char RTTY_LTRS = 0x1F;
static const char RTTY_FIGS = 0x1B;

static const size_t RTTY_ENCODE_CRC_LEN = 4;

/// @brief Decides if a control character needs to be send before the char
/// @param input the next char to be sent
/// @param isFigure The current mode
/// @return Returns 0x00 if no switching is needed. Otherwise RTTY_LTRS or RTTY_FIGS;
char rtty_encode_needs_switching(char input, bool isFigure);

/// @brief Converts an ascii char to baudot code. Assumes the right mode.
/// @param input The char to encode
/// @return The baudot code for the char
char rtty_encode_letter(char input);

/// @brief Calculates the CRC16
/// @param input the baudot chars to generate the crc for
/// @param len the length of the input buffer
/// @return the crc16
uint16_t rtty_crc(char *input, size_t len);

/// @brief Encodes the crc as baudot-compatible code
/// @param input the crc to encode
/// @param buffer a buffer with at least 4 chars
void rtty_encode_crc(uint16_t input, char *buffer);
