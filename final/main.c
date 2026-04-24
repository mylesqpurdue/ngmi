#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "timer.h"
#include "button.h"
#include "simon_says.h"
#include "chardisp.h"
#include "peer_link.h"
#include "pico/multicore.h"

// LCD on SPI0 — pins chosen to avoid all other hardware
// (SPI1 gpio 13-15 = 7-seg, gpio 5-12 = Simon Says, gpio 17/33/34/37-39 = button module)
const int SPI_DISP_SCK = 22;
const int SPI_DISP_CSn = 21;
const int SPI_DISP_TX  = 23;

#define STRIKE1 26
#define STRIKE2 27
#define STRIKE3 28
// 7-seg functions from display.c
void init_sevenseg_spi(void);
void init_sevenseg_dma(void);
void sevenseg_display(const char *str);
void button_isr(void);
void update_strike_leds();

// ─── Variables required by button.h ──────────────────────────────────────────
struct repeating_timer blink_timer;
volatile bool is_blinking  = false;
volatile bool blink_on     = true;
volatile bool current_r    = false;
volatile bool current_g    = false;
volatile bool current_b    = false;
volatile bool module_complete = false;

int parity       = 0;
volatile int strike_count = 0;

// Peer module flags (set by core 1 from UART events)
volatile bool krish_solved = false;
volatile bool myles_solved = false;

// Serial code shown on 7-seg display
char serial_code[4] = "AAA";
static const char letters[9] = {'A', 'C', 'E', 'F', 'H', 'J', 'L', 'P', 'U'};

extern char font[];

//strike leds
void init_strike_leds(){
    gpio_init(STRIKE1);
    gpio_init(STRIKE2);
    gpio_init(STRIKE3);

    gpio_set_dir(STRIKE1, true);
    gpio_set_dir(STRIKE2, true);
    gpio_set_dir(STRIKE3, true);


}
void update_strike_leds(){
    switch (strike_count) {
        case 0:
            gpio_put(STRIKE1, 0);
            gpio_put(STRIKE2, 0);
            gpio_put(STRIKE3, 0);
            break;
        case 1: 
            gpio_put(STRIKE1, 1);
            break;
        case 2: 
            gpio_put(STRIKE2, 1);
            break;
        case 3: 
            gpio_put(STRIKE3, 1);
            break;
        default:
            break;
    }
}

// Cap strikes at 3 and trip the explosion path. Safe from both cores.
static inline void register_strike(const char *who) {
    if (strike_count < 3) {
        strike_count++;
        update_strike_leds();
    }
    printf("[STRIKE] from %s (total %d)\n", who ? who : "?", strike_count);
    if (strike_count >= 3) {
        countdown_secs = 0;
        timer_active = false;
    }
}

// ─── Serial code generation ───────────────────────────────────────────────────
static void generate_serial_code(void) {
    uint32_t r;
    r = get_rand_32(); serial_code[0] = letters[r % 9];
    r = get_rand_32(); serial_code[1] = letters[r % 9];
    r = get_rand_32(); serial_code[2] = '0' + (r % 10);
    parity = (int)(serial_code[2] - '0') % 2;
    serial_code[3] = '\0';
}

// ─── IRQ callback (required by button.h / button_init) ───────────────────────
void irq_callback(uint gpio, uint32_t events) {
    if (gpio == RESET_PIN && (events & GPIO_IRQ_EDGE_FALL))
        reset_isr();
    if (gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL))
        button_isr();
    busy_wait_ms(500);
}
void button_isr() {
    if (module_complete) return;

    int required_digit = get_required_digit();
    int mins = countdown_secs / 60;
    int secs = countdown_secs % 60;

    if (time_contains_digit(required_digit)) {
        gpio_put(CORRECT_LED, 1);
        current_r = 0;
        current_g = 0;
        current_b = 0;
        apply_led_state(true);
        module_complete = true;
        printf("Module complete!\n");
    } else {
        register_strike("BUTTON");
        printf("Strike %d! Required digit: %d, Time was %d:%02d\n",
               strike_count, required_digit, mins, secs);
    }
}

// ─── Reset ISR — arms the game ────────────────────────────────────────────────
void reset_isr(void) {
    countdown_secs  = 300;
    timer_active    = true;
    update_display  = true;
    strike_count    = 0;
    update_strike_leds();
    module_complete = false;
    krish_solved    = false;
    myles_solved    = false;
    gpio_put(CORRECT_LED, 0);
    cancel_repeating_timer(&blink_timer);
    generate_serial_code();

    uint32_t button_type = get_rand_32() % 10;
    current_r = (button_type % 5 == 0 || button_type % 5 == 3 || button_type % 5 == 4);
    current_g = (button_type % 5 == 1 || button_type % 5 == 3 || button_type % 5 == 4);
    current_b = (button_type % 5 == 2 || button_type % 5 == 4);
    is_blinking = (bool)(button_type / 5);

    if (is_blinking) {
        blink_on = true;
        apply_led_state(true);
        add_repeating_timer_ms(BLINK_INTERVAL_MS, led_blink_callback, NULL, &blink_timer);
    } else {
        apply_led_state(true);
    }
}

// ─── Reset pin init ───────────────────────────────────────────────────────────
static void reset_init(void) {
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_IN);
    gpio_pull_up(RESET_PIN);
    gpio_set_irq_enabled_with_callback(RESET_PIN, GPIO_IRQ_EDGE_FALL, true, &irq_callback);
}

// ─── 7-seg helper ─────────────────────────────────────────────────────────────
static void update_sevenseg(void) {
    char buf[9];
    int mins = countdown_secs / 60;
    int secs = countdown_secs % 60;
    snprintf(buf, sizeof(buf), "%s%2d-%02d", serial_code, mins, secs);
    sevenseg_display(buf);
    update_display = false;
}

// ─── Core 1: continuous timer display + peer UART drain ──────────────────────
static void core1_entry(void) {
    peer_link_init();
    peer_event_t evt;

    while (1) {
        if (update_display)
            update_sevenseg();

        while (peer_link_poll(&evt)) {
            if (evt.type == PEER_EVT_STRIKE) {
                register_strike(evt.payload);
            } else if (evt.type == PEER_EVT_SOLVED) {
                if (strcmp(evt.payload, "KRISH") == 0) {
                    krish_solved = true;
                    printf("[PEER] KRISH solved\n");
                } else if (strcmp(evt.payload, "MYLES") == 0) {
                    myles_solved = true;
                    printf("[PEER] MYLES solved\n");
                }
            }
        }

        sleep_ms(10);
    }
}

// ─── Strike flash (all Simon Says LEDs) ──────────────────────────────────────
static void ss_strike_flash(void) {
    for (int i = 0; i < 3; i++) {
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
        gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
        sleep_ms(200);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
        sleep_ms(200);
    }
}

// Game-over check shared by every wait loop in main.
#define BOMB_DEAD() (countdown_secs <= 0 || strike_count >= 3)

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(void) {
    stdio_init_all();

    // Celine's hardware
    init_sevenseg_spi();
    init_sevenseg_dma();
    reset_init();
    button_init();
    rgb_init();
    init_hardware_timer();
    init_strike_leds();

    // Alexia's hardware
    simon_says_init();
    init_chardisp_pins();
    cd_init();

    multicore_launch_core1(core1_entry);

    char strike_line[17];

    while (1) {
        // Catch stale bomb death (e.g. 3-strike-mid-Simon-fail)
        if (BOMB_DEAD()) goto exploded;

        // ── IDLE: wait for RESET button press ───────────────────────────────
        simon_says_reset();
        cd_display1("Press RESET btn ");
        cd_display2("to start game!  ");

        while (!timer_active) {
            sleep_ms(10);
        }

        // ── Wait for red button to start Simon Says ──────────────────────────
        cd_display1("Press RED btn   ");
        cd_display2("to start Simon! ");
        while (gpio_get(SS_BTN_RED)) {
            if (BOMB_DEAD()) goto exploded;
            sleep_ms(10);
        }
        simon_says_startup_animation();
        while (!gpio_get(SS_BTN_RED)) {
            if (BOMB_DEAD()) goto exploded;
            sleep_ms(5);
        }

        // ── ACTIVE: game is armed ────────────────────────────────────────────

        // ── Round 1: Regular Simon Says ──────────────────────────────────────
        cd_display1("- Round 1 -     ");
        cd_display2("Regular Round   ");
        sleep_ms(2500);

        cd_display1("Watch carefully!");
        cd_display2("                ");
        simon_says_demo(6);

        if (BOMB_DEAD()) goto exploded;

        cd_display1("Your turn!      ");
        cd_display2("Repeat sequence ");

        if (!simon_says_collect_input(6)) {
            if (BOMB_DEAD()) goto exploded;
            register_strike("SIMON");
            if (BOMB_DEAD()) goto exploded;
            cd_display1("Wrong! Strike!  ");
            snprintf(strike_line, sizeof(strike_line), "Strikes: %d      ", strike_count);
            cd_display2(strike_line);
            ss_strike_flash();
            sleep_ms(1500);
            continue;
        }

        if (BOMB_DEAD()) goto exploded;

        cd_display1("Correct!        ");
        cd_display2("Round 1 cleared!");
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
        gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
        sleep_ms(2000);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);

        // ── Round 2: Color Round ─────────────────────────────────────────────
        cd_display1("- Round 2 -     ");
        cd_display2("Color Round     ");
        sleep_ms(2000);
        cd_display1("Use your manual!");
        cd_display2("Press when ready");

        while (gpio_get(SS_BTN_RED) && gpio_get(SS_BTN_GREEN) &&
               gpio_get(SS_BTN_BLUE) && gpio_get(SS_BTN_YELLOW)) {
            if (BOMB_DEAD()) goto exploded;
            sleep_ms(1);
        }
        while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
               !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) {
            if (BOMB_DEAD()) goto exploded;
            sleep_ms(10);
        }
        sleep_ms(300);

        if (BOMB_DEAD()) goto exploded;

        simon_says_generate(6);

        static const char code_letters[] = "ABCD";
        char code_line[17];
        snprintf(code_line, sizeof(code_line), "Code: %c         ", code_letters[simon_says_get_code()]);
        cd_display1(code_line);
        cd_display2("Find your code! ");
        sleep_ms(5000);

        cd_display1(code_line);
        cd_display2("Press mapped btn");

        if (!simon_says_color_round(6)) {
            if (BOMB_DEAD()) goto exploded;
            register_strike("SIMON");
            if (BOMB_DEAD()) goto exploded;
            cd_display1("Wrong! Strike!  ");
            snprintf(strike_line, sizeof(strike_line), "Strikes: %d      ", strike_count);
            cd_display2(strike_line);
            ss_strike_flash();
            sleep_ms(1500);
            continue;
        }

        if (BOMB_DEAD()) goto exploded;

        // ── Simon done — wait for button + KRISH + MYLES ──────────────────────
        if (module_complete && krish_solved && myles_solved) goto defused;

        cd_display1("Simon: DONE!    ");
        cd_display2("Waiting others..");

        while (!(module_complete && krish_solved && myles_solved)) {
            if (BOMB_DEAD()) goto exploded;
            sleep_ms(10);
        }

        defused:
        cd_display1("** DEFUSED! **  ");
        cd_display2("All modules OK! ");
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
        gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
        timer_active = false;
        for (;;);

        exploded:
        cd_display1("** BOOM! **     ");
        cd_display2("Game over!      ");
        for (int i = 0; i < 10; i++) {
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            sleep_ms(100);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            sleep_ms(100);
        }
        for (;;);
    }

    return 0;
}