#include "seg7.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include <string.h>
#include <stdio.h>

// ─── 7-segment font (ASCII → segment pattern) ────────────────────────────────
// Bit order: dp g f e d c b a  (bit 7 = decimal point, bit 0 = segment a)
char font[128] = {
    [' '] = 0x00,
    ['-'] = 0x40,
    ['_'] = 0x08,
    ['0'] = 0x3F, ['1'] = 0x06, ['2'] = 0x5B, ['3'] = 0x4F,
    ['4'] = 0x66, ['5'] = 0x6D, ['6'] = 0x7D, ['7'] = 0x07,
    ['8'] = 0x7F, ['9'] = 0x6F,
    ['A'] = 0x77, ['B'] = 0x7C, ['C'] = 0x39, ['D'] = 0x5E,
    ['E'] = 0x79, ['F'] = 0x71, ['G'] = 0x3D, ['H'] = 0x76,
    ['I'] = 0x06, ['J'] = 0x1E, ['L'] = 0x38, ['N'] = 0x54,
    ['O'] = 0x3F, ['P'] = 0x73, ['R'] = 0x50, ['S'] = 0x6D,
    ['T'] = 0x78, ['U'] = 0x3E, ['Y'] = 0x6E,
    ['a'] = 0x77, ['b'] = 0x7C, ['c'] = 0x58, ['d'] = 0x5E,
    ['e'] = 0x79, ['f'] = 0x71, ['g'] = 0x6F, ['h'] = 0x74,
    ['i'] = 0x06, ['j'] = 0x1E, ['l'] = 0x38, ['n'] = 0x54,
    ['o'] = 0x5C, ['p'] = 0x73, ['r'] = 0x50, ['s'] = 0x6D,
    ['t'] = 0x78, ['u'] = 0x1C, ['y'] = 0x6E,
};

// ─── Display buffer ──────────────────────────────────────────────────────────
// Aligned to 16 bytes for DMA compatibility (matches lab requirement).
// Format per entry: bits[10:8] = position, bits[7:0] = segment pattern.
uint16_t __attribute__((aligned(16))) msg[8] = {
    (0 << 8) | 0x00,
    (1 << 8) | 0x00,
    (2 << 8) | 0x00,
    (3 << 8) | 0x00,
    (4 << 8) | 0x00,
    (5 << 8) | 0x00,
    (6 << 8) | 0x00,
    (7 << 8) | 0x00,
};

// ─── SPI init (mirrors display_init_spi from lab) ────────────────────────────

static void _spi_init(void) {
    gpio_set_function(SEG7_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SEG7_TX_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(SEG7_CSN_PIN, GPIO_FUNC_SPI);

    spi_init(spi1, 125000);
    spi_set_format(spi1, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

// ─── String → msg[] (mirrors display_char_print from lab) ───────────────────

void seg7_print(const char *str) {
    int dp_found = 0;
    int out_idx  = 0;

    for (int i = 0; i < 8 && str[i] != '\0'; i++) {
        if (str[i] == '.') {
            if (dp_found) continue;
            if (out_idx > 0) {
                msg[out_idx - 1] |= (1 << 7);   // set dp bit on previous digit
            }
            dp_found = 1;
        } else {
            uint16_t seg = (uint16_t)(uint8_t)font[(unsigned char)str[i]];
            seg |= (uint16_t)((out_idx & 0x7) << 8);  // embed position
            msg[out_idx] = seg;
            out_idx++;
        }
    }
    // Blank any remaining positions
    for (; out_idx < 8; out_idx++) {
        msg[out_idx] = (uint16_t)(out_idx << 8);
    }
}

// ─── Public API ─────────────────────────────────────────────────────────────

void seg7_init(void) {
    _spi_init();
    seg7_clear();
    seg7_refresh();
}

void seg7_display_time(uint8_t minutes, uint8_t seconds, bool separator_on) {
    char buf[10];
    if (separator_on) {
        // "  MM.SS " — decimal point acts as minute/second separator
        snprintf(buf, sizeof(buf), "  %02d.%02d", minutes, seconds);
    } else {
        // Separator off: show time with a space gap so it visually blinks
        snprintf(buf, sizeof(buf), "  %02d %02d", minutes, seconds);
    }
    seg7_print(buf);
}

void seg7_show_dashes(void) {
    seg7_print("--------");
}

void seg7_clear(void) {
    for (int i = 0; i < 8; i++) {
        msg[i] = (uint16_t)(i << 8);   // position bits only, segments off
    }
}

// Write current msg[] to the display over SPI (mirrors display_print from lab).
void seg7_refresh(void) {
    for (int i = 0; i < 8; i++) {
        spi_write16_blocking(spi1, &msg[i], 1);
    }
}
