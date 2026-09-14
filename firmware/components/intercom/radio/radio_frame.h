#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct radio_frame {
    size_t len;
    char *content;
} radio_frame;

/// @brief Generates a radio frame
/// @param input the input to encode
/// @param len the input buffer length
/// @param append_crc if true, appends a crc to the radio frame
/// @return The radio frame
radio_frame radio_encode_frame(char *input, size_t len, bool append_crc);
