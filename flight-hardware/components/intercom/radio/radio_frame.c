#include "radio_frame.h"
#include "rtty.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static const char prefix[] = "DA0FRA HIGH ALTITUDE BALLOON ";
static const size_t prefix_len = sizeof(prefix) - 1;
static const char postfix[] = " DA0FRA";
static const size_t postfix_len = sizeof(postfix) - 1;

typedef struct {
    size_t len;
    bool is_figs;
} size_and_figs;

static size_and_figs write_rtty_to_buffer(const char *input, size_t len, char *output, bool start_figs);

radio_frame radio_encode_frame(char *input, size_t len, bool append_crc) {
    radio_frame message = {0};
    /*
     * Worst case:
     * - allways switch between LTRS and FIGS -> two symbols per letter
     * - CRC: Constant 4 symbols + 1 SPACE symbol
     * - Framing CR LF at start and end: constant 4 symbols
     * We'll run realloc() to not waste that much memory
     */
    message.content = malloc((len + prefix_len + postfix_len) * 2 + append_crc * (RTTY_ENCODE_CRC_LEN + 1) + 4);
    size_t pos = 3;
    bool is_figure = false;

    if (rtty_encode_needs_switching(prefix[0], false)) {
        message.content[0] = RTTY_FIGS;
        is_figure = true;
    } else {
        message.content[0] = RTTY_LTRS;
    }

    message.content[1] = rtty_encode_letter('\r');
    message.content[2] = rtty_encode_letter('\n');

    size_and_figs prefix_stats = write_rtty_to_buffer(prefix, prefix_len, &message.content[pos], is_figure);
    pos += prefix_stats.len;
    is_figure = prefix_stats.is_figs;

    size_t pos_content = pos;

    size_and_figs content_stats = write_rtty_to_buffer(input, len, &message.content[pos], is_figure);
    pos += content_stats.len;
    is_figure = content_stats.is_figs;

    if (append_crc) {
        uint16_t crc = rtty_crc(&message.content[pos_content], pos - pos_content);
        message.content[pos] = rtty_encode_letter(' ');
        ++pos;
        rtty_encode_crc(crc, &message.content[pos]);
        pos += RTTY_ENCODE_CRC_LEN;
    }

    size_and_figs postfix_stats = write_rtty_to_buffer(postfix, postfix_len, &message.content[pos], is_figure);
    pos += postfix_stats.len;

    message.content[pos] = rtty_encode_letter('\r');
    message.content[pos + 1] = rtty_encode_letter('\n');
    pos += 2;

    message.len = pos;
    void *newBuff = realloc(message.content, pos);
    if (newBuff != NULL) {
        message.content = newBuff;
    }
    return message;
}

static size_and_figs write_rtty_to_buffer(const char *input, const size_t len, char *output, const bool start_figs) {
    size_and_figs stats = {.len = 0, .is_figs = start_figs};
    for (size_t i = 0; i < len; i++) {
        char curr_letter = input[i];
        char switch_char = rtty_encode_needs_switching(curr_letter, stats.is_figs);
        if (switch_char) {
            output[stats.len] = switch_char;
            stats.is_figs = !stats.is_figs;
            ++stats.len;
        }
        char curr_word = rtty_encode_letter(curr_letter);
        output[stats.len] = curr_word;
        ++stats.len;
    }

    return stats;
}
