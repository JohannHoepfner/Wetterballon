#include "rtty.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint16_t calc_crc(uint8_t const message[], int nBytes);

static const char ALPHABET_LUT[26] = {
    0x03, // A
    0x19, // B
    0x0E, // C
    0x09, // D
    0x01, // E
    0x0D, // F
    0x1A, // G
    0x14, // H
    0x06, // I
    0x0B, // J
    0x0F, // K
    0x12, // L
    0x1C, // M
    0x0C, // N
    0x18, // O
    0x16, // P
    0x17, // Q
    0x0A, // R
    0x05, // S
    0x10, // T
    0x07, // U
    0x1E, // V
    0x13, // W
    0x1D, // X
    0x15, // Y
    0x11  // Z
};

static const char DIGIT_LUT[10] = {
    0x16, // 0
    0x17, // 1
    0x13, // 2
    0x01, // 3
    0x0A, // 4
    0x10, // 5
    0x15, // 6
    0x07, // 7
    0x06, // 8
    0x18  // 9
};

static const char OTHERSHIT_LUT[] = {
    [' '] = 0x04, ['\r'] = 0x08, ['\n'] = 0x02, [','] = 0x0C, ['-'] = 0x03,  ['\''] = 0x05, ['$'] = 0x09,
    ['!'] = 0x0D, [':'] = 0x0E,  ['('] = 0x0F,  [')'] = 0x12, ['+'] = 0x11,  ['#'] = 0x14,  ['?'] = 0x19,
    ['&'] = 0x1A, ['.'] = 0x1C,  ['/'] = 0x1D,  ['='] = 0x1E, ['\a'] = 0x0B,
};

char
rtty_encode_needs_switching(char input, bool isFigure) {
    input = toupper(input);

    if (input >= 'A' && input <= 'Z') {
        return isFigure ? RTTY_LTRS : 0x00;
    }

    if (input >= '0' && input <= '9') {
        return isFigure ? 0x00 : RTTY_FIGS;
    }

    if (input == ' ' || input == '\r' || input == '\n') {
        return 0x00;
    }

    switch (input) {
    case '-':
    case ',':
    case '\'':
    case '$':
    case '!':
    case ':':
    case '(':
    case ')':
    case '+':
    case '#':
    case '?':
    case '&':
    case '.':
    case '/':
    case '=':
    case '\a':
        return isFigure ? 0x00 : RTTY_FIGS;
    default:
        return 0x00;
    }
}

char
rtty_encode_letter(char input) {
    if (input >= 'a' && input <= 'z') {
        input -= ('a' - 'A');
    }

    if (input >= 'A' && input <= 'Z') {
        return ALPHABET_LUT[input - 'A'];
    }

    if (input >= '0' && input <= '9') {
        return DIGIT_LUT[input - '0'];
    }

    return OTHERSHIT_LUT[(unsigned long)input];
}

uint16_t
rtty_crc(char *input, size_t len) {
    size_t buf_size = (5 * len) / 8 + 1;
    uint8_t buffer[buf_size];
    memset(buffer, 0, buf_size);
    size_t bit_pos = 0;

    // calculate buffer
    for (size_t i = 0; i < len; i++) {
        uint8_t val = input[i];
        size_t byte_idx = bit_pos / 8;
        int bit_offset = bit_pos % 8;

        uint16_t shifted = (uint16_t)val << bit_offset;
        buffer[byte_idx] |= (uint8_t)(shifted & 0xFF);
        if (bit_offset > 3) {
            buffer[byte_idx + 1] |= (uint8_t)(shifted >> 8);
        }

        bit_pos += 5;
    }

    // calculate crc
    uint16_t crc = calc_crc(buffer, buf_size);

    return crc;
}

void
rtty_encode_crc(uint16_t input, char *buffer) {
    for (char i = 0; i < RTTY_ENCODE_CRC_LEN; i++) {
        uint8_t current = (input >> 12) & 0xF;
        char currentResult = ((current << 1) & 0b11100) | (current & 1) | (((current >> 2) & 2) ^ ((current >> 1) & 2));
        buffer[(unsigned long)i % 0xFF] = currentResult;
        input = input << 4;
    }
}

#define WIDTH (16)
#define TOPBIT (1 << (WIDTH - 1))
#define POLY (0x1021)

static uint16_t
calc_crc(uint8_t const message[], int nBytes) {
    uint16_t remainder = 0;

    for (int byte = 0; byte < nBytes; ++byte) {
        remainder ^= (message[byte] << (WIDTH - 8));

        for (uint8_t bit = 8; bit > 0; --bit) {
            if (remainder & TOPBIT) {
                remainder = (remainder << 1) ^ POLY;
            } else {
                remainder = (remainder << 1);
            }
        }
    }

    return (remainder);
} /* calc_crc() */
