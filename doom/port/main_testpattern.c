/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>

#include "fcbus_device.h"
#include "hardware/flash.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "cli.h"
#include "flash_layout.h"
#include "trace.h"

extern const unsigned char fcpico_bootrom[];
extern const int fcpico_bootrom_length;

static fcbus_device_t bus;
static volatile uint32_t selected_pattern;

static uint8_t pattern_pixel(uint32_t pattern, uint32_t frame, int x, int y) {
    switch (pattern) {
    case 0:
        return (uint8_t)((x / 34) & 3);
    case 1:
        return (uint8_t)(((x / 8) ^ (y / 8) ^ (frame / 30)) & 3);
    case 2:
        return (uint8_t)(((x / 16) + (frame / 15)) & 3);
    default:
        if (y < 16) {
            return (uint8_t)((frame >> ((x / 16) & 7)) & 3);
        }
        return (uint8_t)(((x + y + (int)frame) >> 3) & 3);
    }
}

static void build_pattern(uint16_t *stream, uint32_t pattern, uint32_t frame) {
    for (int y = 0; y < VRAM_LINES; ++y) {
        for (int tile = 0; tile < VRAM_LINE_WORDS; ++tile) {
            uint16_t word = 0;
            for (int pixel = 0; pixel < 8; ++pixel) {
                uint8_t colour = pattern_pixel(pattern, frame, tile * 8 + pixel, y);
                word = (uint16_t)((word << 1) | (colour & 1u) | ((colour & 2u) << 7));
            }
            *fcbus_stream_word(stream, y, tile) = word;
        }
    }
}

int main(void) {
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    pico_get_unique_board_id_string(board_id, sizeof board_id);
    uint32_t flash_size = flash_devinfo_size_to_bytes(flash_devinfo_get_cs_size(0));
    printf("FC PICO test pattern\nUnique ID: %s\nflash=%u expected=%u\n", board_id,
           (unsigned)flash_size, (unsigned)FLASH_TOTAL_BYTES);
    if (fcpico_bootrom_length != FCBUS_ROM_INES_HDR + FCBUS_ROM_PRG_BYTES) {
        panic("invalid tutorial boot ROM length");
    }

    fcbus_config_t config = {
        .rom_image = fcpico_bootrom + FCBUS_ROM_INES_HDR,
        .proto_default = FCBUS_PROTO_V1,
    };
    fcbus_device_init(&bus, &config);
    bool trace_available = trace_init();
    printf("trace=%s\n", trace_available ? "ready" : "unavailable");

    cli_t cli;
    cli_init(&cli, &bus, &selected_pattern);
    uint32_t last_frame = UINT32_MAX;
    while (true) {
        fcbus_device_poll(&bus);
        uint32_t frame = fcbus_device_stats(&bus)->frames;
        if (frame != last_frame && fcbus_device_back_is_free(&bus)) {
            uint16_t *stream = fcbus_device_stream_back(&bus);
            build_pattern(stream, selected_pattern, frame);
            uint8_t palette[16] = {0x0f, 0x00, 0x10, 0x20, 0x0f, 0x06, 0x16, 0x26,
                                   0x0f, 0x09, 0x19, 0x29, 0x0f, 0x01, 0x21, 0x31};
            uint8_t attributes[64];
            for (unsigned index = 0; index < sizeof attributes; ++index) {
                attributes[index] = (uint8_t)((index + frame / 30) & 1u ? 0x1b : 0xe4);
            }
            fcbus_core_palette(&bus.core, palette);
            fcbus_core_attr_table(&bus.core, attributes);
            fcbus_device_publish(&bus);
            last_frame = frame;
            gpio_put(PICO_DEFAULT_LED_PIN, (frame >> 4) & 1u);
        }
        cli_poll(&cli);
    }
}
