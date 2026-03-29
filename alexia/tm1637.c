#include "tm1637.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// ─── Segment encoding (a–g, common cathode) ─────────────────────────────────
static const uint8_t DIGIT_SEGS[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
};

static uint _clk;
static uint _dio;
static uint8_t _brightness = 7;

// ─── Low-level bit-bang helpers ──────────────────────────────────────────────

static inline void clk_high(void) { gpio_set_dir(_clk, GPIO_IN);  }
static inline void clk_low(void)  { gpio_set_dir(_clk, GPIO_OUT); gpio_put(_clk, 0); }
static inline void dio_high(void) { gpio_set_dir(_dio, GPIO_IN);  }
static inline void dio_low(void)  { gpio_set_dir(_dio, GPIO_OUT); gpio_put(_dio, 0); }

static void start(void) {
    dio_high(); clk_high(); sleep_us(2);
    dio_low();  sleep_us(2);
    clk_low();  sleep_us(2);
}

static void stop(void) {
    clk_low(); sleep_us(2);
    dio_low(); sleep_us(2);
    clk_high(); sleep_us(2);
    dio_high(); sleep_us(2);
}

static bool write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        clk_low(); sleep_us(2);
        if (byte & 0x01) dio_high(); else dio_low();
        sleep_us(2);
        clk_high(); sleep_us(2);
        byte >>= 1;
    }
    // ACK
    clk_low(); sleep_us(5);
    dio_high();
    clk_high(); sleep_us(2);
    bool ack = !gpio_get(_dio);
    clk_low(); sleep_us(2);
    return ack;
}

// ─── Public API ─────────────────────────────────────────────────────────────

void tm1637_init(uint clk_pin, uint dio_pin) {
    _clk = clk_pin;
    _dio = dio_pin;
    gpio_init(_clk); gpio_set_dir(_clk, GPIO_OUT); gpio_put(_clk, 0);
    gpio_init(_dio); gpio_set_dir(_dio, GPIO_OUT); gpio_put(_dio, 0);
    sleep_ms(1);
    tm1637_clear();
}

void tm1637_set_brightness(uint8_t level) {
    if (level > 7) level = 7;
    _brightness = level;
}

static void write_segments(uint8_t segs[4]) {
    // Command 1: data write, auto-increment address
    start();
    write_byte(0x40);
    stop();

    // Command 2: set start address 0
    start();
    write_byte(0xC0);
    for (int i = 0; i < 4; i++) write_byte(segs[i]);
    stop();

    // Command 3: display on + brightness
    start();
    write_byte(0x88 | (_brightness & 0x07));
    stop();
}

void tm1637_display_time(uint8_t minutes, uint8_t seconds, bool colon) {
    uint8_t segs[4];
    segs[0] = DIGIT_SEGS[minutes / 10];
    segs[1] = DIGIT_SEGS[minutes % 10];
    if (colon) segs[1] |= 0x80;    // bit 7 lights the colon on pos 1
    segs[2] = DIGIT_SEGS[seconds / 10];
    segs[3] = DIGIT_SEGS[seconds % 10];
    write_segments(segs);
}

void tm1637_display_number(uint16_t number) {
    if (number > 9999) number = 9999;
    uint8_t segs[4];
    segs[3] = DIGIT_SEGS[number % 10]; number /= 10;
    segs[2] = DIGIT_SEGS[number % 10]; number /= 10;
    segs[1] = DIGIT_SEGS[number % 10]; number /= 10;
    segs[0] = DIGIT_SEGS[number % 10];
    write_segments(segs);
}

void tm1637_clear(void) {
    uint8_t segs[4] = {0, 0, 0, 0};
    write_segments(segs);
}
