#include "simon_says.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>

extern volatile int strike_count;
extern int countdown_secs;
#define BOMB_DEAD() (countdown_secs <= 0 || strike_count >= 3)

static uint8_t s_demo_sequence[8];
static int     s_demo_length    = 0;
static uint8_t s_user_input[8];
static int     s_user_input_len = 0;
static uint8_t s_serial_code    = 0;  // 0=A, 1=B, 2=C, 3=D

// Manual lookup table: rule_map[code][led-1] = button to press
// Colors: 1=RED 2=GREEN 3=BLUE 4=YELLOW (stored as index 0-3)
// Code A: Red->Green,  Green->Red,    Blue->Yellow, Yellow->Blue
// Code B: Red->Yellow, Green->Blue,   Blue->Green,  Yellow->Red
// Code C: Red->Blue,   Green->Yellow, Blue->Red,    Yellow->Green
// Code D: Red->Red,    Green->Blue,   Blue->Yellow, Yellow->Green
static const uint8_t rule_map[4][4] = {
    {2, 1, 4, 3},
    {4, 3, 2, 1},
    {3, 4, 1, 2},
    {1, 3, 4, 2},
};

void simon_says_init(void) {
    gpio_init(SS_LED_RED);    gpio_set_dir(SS_LED_RED,    GPIO_OUT); gpio_put(SS_LED_RED,    0);
    gpio_init(SS_LED_GREEN);  gpio_set_dir(SS_LED_GREEN,  GPIO_OUT); gpio_put(SS_LED_GREEN,  0);
    gpio_init(SS_LED_BLUE);   gpio_set_dir(SS_LED_BLUE,   GPIO_OUT); gpio_put(SS_LED_BLUE,   0);
    gpio_init(SS_LED_YELLOW); gpio_set_dir(SS_LED_YELLOW, GPIO_OUT); gpio_put(SS_LED_YELLOW, 0);

    gpio_init(SS_BTN_RED);    gpio_set_dir(SS_BTN_RED,    GPIO_IN); gpio_pull_up(SS_BTN_RED);
    gpio_init(SS_BTN_GREEN);  gpio_set_dir(SS_BTN_GREEN,  GPIO_IN); gpio_pull_up(SS_BTN_GREEN);
    gpio_init(SS_BTN_BLUE);   gpio_set_dir(SS_BTN_BLUE,   GPIO_IN); gpio_pull_up(SS_BTN_BLUE);
    gpio_init(SS_BTN_YELLOW); gpio_set_dir(SS_BTN_YELLOW, GPIO_IN); gpio_pull_up(SS_BTN_YELLOW);
}

void simon_says_startup_animation(void) {
    for (int cycle = 0; cycle < 2; cycle++) {
        gpio_put(SS_LED_RED, 1);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); sleep_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 1); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); sleep_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 0); sleep_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 1); sleep_ms(120);
    }
    gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
    sleep_ms(80);
    for (int i = 0; i < 3; i++) {
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1); gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1); sleep_ms(150);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); sleep_ms(150);
    }
    sleep_ms(200);
}

void simon_says_demo(int length) {
    uint8_t pool[4] = {1, 2, 3, 4};
    for (int i = 3; i > 0; i--) {
        int j = (int)(time_us_32() % (uint32_t)(i + 1));
        uint8_t tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    for (int i = 0; i < length; i++)
        s_demo_sequence[i] = (i < 4) ? pool[i] : (uint8_t)(time_us_32() % 4 + 1);

    s_serial_code    = (uint8_t)(time_us_32() % 4);
    s_demo_length    = length;
    s_user_input_len = 0;

    for (int i = 0; i < length; i++) {
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);

        if (s_demo_sequence[i] == 1) gpio_put(SS_LED_RED,    1);
        if (s_demo_sequence[i] == 2) gpio_put(SS_LED_GREEN,  1);
        if (s_demo_sequence[i] == 3) gpio_put(SS_LED_BLUE,   1);
        if (s_demo_sequence[i] == 4) gpio_put(SS_LED_YELLOW, 1);

        sleep_ms(SS_SHOW_MS);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
        sleep_ms(SS_SHOW_GAP_MS);
    }
}

bool simon_says_collect_input(int length) {
    int step = 0;

    while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
           !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) {
        if (BOMB_DEAD()) return false;
        sleep_ms(10);
    }
    sleep_ms(100);

    while (step < length) {
        if (BOMB_DEAD()) return false;

        if (!gpio_get(SS_BTN_RED)) {
            gpio_put(SS_LED_RED, 1);
            while (!gpio_get(SS_BTN_RED)) {
                if (BOMB_DEAD()) return false;
                sleep_ms(5);
            }
            gpio_put(SS_LED_RED, 0);
            if (s_demo_sequence[step] != 1) return false;
            step++;
            sleep_ms(80);

        } else if (!gpio_get(SS_BTN_GREEN)) {
            gpio_put(SS_LED_GREEN, 1);
            while (!gpio_get(SS_BTN_GREEN)) {
                if (BOMB_DEAD()) return false;
                sleep_ms(5);
            }
            gpio_put(SS_LED_GREEN, 0);
            if (s_demo_sequence[step] != 2) return false;
            step++;
            sleep_ms(80);

        } else if (!gpio_get(SS_BTN_BLUE)) {
            gpio_put(SS_LED_BLUE, 1);
            while (!gpio_get(SS_BTN_BLUE)) {
                if (BOMB_DEAD()) return false;
                sleep_ms(5);
            }
            gpio_put(SS_LED_BLUE, 0);
            if (s_demo_sequence[step] != 3) return false;
            step++;
            sleep_ms(80);

        } else if (!gpio_get(SS_BTN_YELLOW)) {
            gpio_put(SS_LED_YELLOW, 1);
            while (!gpio_get(SS_BTN_YELLOW)) {
                if (BOMB_DEAD()) return false;
                sleep_ms(5);
            }
            gpio_put(SS_LED_YELLOW, 0);
            if (s_demo_sequence[step] != 4) return false;
            step++;
            sleep_ms(80);
        } else {
            sleep_ms(5);
        }
    }
    return true;
}

void simon_says_generate(int length) {
    uint8_t pool[4] = {1, 2, 3, 4};
    for (int i = 3; i > 0; i--) {
        int j = (int)(time_us_32() % (uint32_t)(i + 1));
        uint8_t tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    for (int i = 0; i < length; i++)
        s_demo_sequence[i] = (i < 4) ? pool[i] : (uint8_t)(time_us_32() % 4 + 1);

    s_serial_code    = (uint8_t)(time_us_32() % 4);
    s_demo_length    = length;
    s_user_input_len = 0;
}

bool simon_says_color_round(int length) {
    while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
           !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) {
        if (BOMB_DEAD()) return false;
        sleep_ms(10);
    }
    sleep_ms(100);

    for (int step = 0; step < length; step++) {
        int led_pin = (s_demo_sequence[step] == 1) ? SS_LED_RED    :
                      (s_demo_sequence[step] == 2) ? SS_LED_GREEN  :
                      (s_demo_sequence[step] == 3) ? SS_LED_BLUE   : SS_LED_YELLOW;
        gpio_put(led_pin, 1);

        uint8_t pressed = 0;
        while (!pressed) {
            if (BOMB_DEAD()) return false;
            if      (!gpio_get(SS_BTN_RED))    pressed = 1;
            else if (!gpio_get(SS_BTN_GREEN))  pressed = 2;
            else if (!gpio_get(SS_BTN_BLUE))   pressed = 3;
            else if (!gpio_get(SS_BTN_YELLOW)) pressed = 4;
            sleep_ms(5);
        }

        gpio_put(led_pin, 0);

        while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
               !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) {
            if (BOMB_DEAD()) return false;
            sleep_ms(5);
        }

        uint8_t expected = rule_map[s_serial_code][s_demo_sequence[step] - 1];
        if (pressed != expected) return false;
        sleep_ms(150);
    }
    return true;
}

uint8_t simon_says_get_code(void) { return s_serial_code; }

bool simon_says_check_input(void) {
    if (s_user_input_len != s_demo_length) return false;
    for (int i = 0; i < s_demo_length; i++)
        if (s_user_input[i] != s_demo_sequence[i]) return false;
    return true;
}

void simon_says_record_input(uint8_t color) {
    if (s_user_input_len < s_demo_length)
        s_user_input[s_user_input_len++] = color;
}

void simon_says_reset(void) {
    gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
    gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
    s_demo_length    = 0;
    s_user_input_len = 0;
    memset(s_demo_sequence, 0, sizeof(s_demo_sequence));
    memset(s_user_input,    0, sizeof(s_user_input));
}

void simon_says_set_active(bool active) { (void)active; }
bool simon_says_update(void)            { return false; }

bool simon_says_selftest(void) {
    bool pass = true;
    uint32_t t;

    gpio_put(SS_LED_RED, 1);
    t = to_ms_since_boot(get_absolute_time());
    while (gpio_get(SS_BTN_RED))
        if (to_ms_since_boot(get_absolute_time()) - t > 5000) { pass = false; break; }
    gpio_put(SS_LED_RED, 0); sleep_ms(300);

    gpio_put(SS_LED_GREEN, 1);
    t = to_ms_since_boot(get_absolute_time());
    while (gpio_get(SS_BTN_GREEN))
        if (to_ms_since_boot(get_absolute_time()) - t > 5000) { pass = false; break; }
    gpio_put(SS_LED_GREEN, 0); sleep_ms(300);

    gpio_put(SS_LED_BLUE, 1);
    t = to_ms_since_boot(get_absolute_time());
    while (gpio_get(SS_BTN_BLUE))
        if (to_ms_since_boot(get_absolute_time()) - t > 5000) { pass = false; break; }
    gpio_put(SS_LED_BLUE, 0); sleep_ms(300);

    gpio_put(SS_LED_YELLOW, 1);
    t = to_ms_since_boot(get_absolute_time());
    while (gpio_get(SS_BTN_YELLOW))
        if (to_ms_since_boot(get_absolute_time()) - t > 5000) { pass = false; break; }
    gpio_put(SS_LED_YELLOW, 0); sleep_ms(300);

    return pass;
}
