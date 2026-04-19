// I ain't finish training the model yet 
// Updated code will come soon 
// Test code rn works for UART Communication 
// This old code is just a placeholder 
// Until I finish debugging my curr

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

// Pin Assignments 
#define UART_PORT uart1
#define UART_TX_PIN 8 // GP8 → ESP32 RX (D7/GPIO44)
#define UART_RX_PIN 9 // GP9 ← ESP32 TX (D6/GPIO43)
#define UART_BAUD 115200

#define LED1_PIN 2 // Pattern LED (bit 0)
#define LED2_PIN 3 // Pattern LED (bit 1)
#define LED3_PIN 4 // Pattern LED (bit 2)

#define BUTTON_PIN 5 // Fallback morse key (active-low, pull-up)
#define BUZZER_PIN 6 // PWM buzzer output
#define DEBUG_LED_PIN 15 // Debug LED — toggles on UART RX

// Timing (ms)
#define DOT_MAX_MS 200     
#define LETTER_TIMEOUT_US 1500000 
#define SUBMIT_TIMEOUT_US 3000000 
#define GAME_TIMEOUT_US 600000000ULL 

// Buzzer Frequencies 
#define TONE_DOT_HZ 880     
#define TONE_DASH_HZ 440     
#define TONE_CORRECT_HZ 1047    
#define TONE_STRIKE_HZ 220     

// Game states 
typedef enum {
    STATE_IDLE,
    STATE_AWAITING_INPUT,
    STATE_CHECKING,
    STATE_DEFUSED,
    STATE_STRIKE,
    STATE_EXPLODED
} game_state_t;

// Morst Table lookup 
typedef struct {
    const char *pattern;
    char letter;
} morse_entry_t;

static const morse_entry_t MORSE_TABLE[] = {
    {".-",   'A'},  {"-...", 'B'},  {"-.-.", 'C'},  {"-..",  'D'},
    {".",    'E'},  {"..-.", 'F'},  {"--.",  'G'},  {"....", 'H'},
    {"..",   'I'},  {".---", 'J'},  {"-.-",  'K'},  {".-..", 'L'},
    {"--",   'M'},  {"-.",   'N'},  {"---",  'O'},  {".--.", 'P'},
    {"--.-", 'Q'},  {".-.",  'R'},  {"...",  'S'},  {"-",    'T'},
    {"..-",  'U'},  {"...-", 'V'},  {".--",  'W'},  {"-..-", 'X'},
    {"-.--", 'Y'},  {"--..", 'Z'}
};
#define MORSE_TABLE_SIZE (sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]))

// Word Bank 
static const char *WORD_BANK[8] = {
    "SOS", "FIRE", "BOMB", "HELP", "WIRE", "CODE", "FUSE", "HALT" 
};

// Global State 
static volatile game_state_t g_state = STATE_IDLE;
static volatile uint8_t g_led_pattern = 0;
static volatile uint8_t g_strikes = 0;
static const char *g_correct_word = NULL;

static char g_morse_buf[8];     
static uint8_t g_morse_idx = 0;
static char g_decoded[16];      
static uint8_t g_word_idx  = 0;

static volatile uint64_t g_last_input_us = 0;
static volatile uint64_t g_game_start_us = 0;
static volatile uint64_t g_btn_press_us = 0;
static volatile bool g_btn_held = false;

#define RX_BUF_SIZE 32
static volatile char g_rx_buf[RX_BUF_SIZE];
static volatile uint8_t g_rx_head = 0;
static volatile uint8_t g_rx_tail = 0;

static void leds_set(uint8_t pattern);

// buzzer 
static void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms) {
    uint slice   = pwm_gpio_to_slice_num(BUZZER_PIN);
    uint channel = pwm_gpio_to_channel(BUZZER_PIN);

    uint32_t sys_clk = 125000000;
    uint16_t divider = 1;
    uint32_t wrap    = sys_clk / freq_hz - 1;
    while (wrap > 65535) {
        divider *= 2;
        wrap = (sys_clk / divider) / freq_hz - 1;
    }

    pwm_set_clkdiv(slice, (float)divider);
    pwm_set_wrap(slice, (uint16_t)wrap);
    pwm_set_chan_level(slice, channel, (uint16_t)(wrap / 2)); 
    pwm_set_enabled(slice, true);

    sleep_ms(duration_ms);

    pwm_set_enabled(slice, false);
    gpio_put(BUZZER_PIN, 0);   
}

static void buzzer_beep_dot(void) { buzzer_tone(TONE_DOT_HZ, 80);  }
static void buzzer_beep_dash(void) { buzzer_tone(TONE_DASH_HZ, 150); }

static void buzzer_victory(void) {
    buzzer_tone(TONE_CORRECT_HZ, 100);
    sleep_ms(40);
    buzzer_tone(TONE_CORRECT_HZ, 100);
    sleep_ms(40);
    buzzer_tone((uint16_t)(TONE_CORRECT_HZ * 1.5), 300);
}

static void buzzer_strike(void) { buzzer_tone(TONE_STRIKE_HZ, 500);  }
static void buzzer_explode(void) { buzzer_tone(TONE_STRIKE_HZ, 1200); }

// Morse Decoder 
static char morse_decode(const char *pattern) {
    for (uint8_t i = 0; i < MORSE_TABLE_SIZE; i++) {
        if (strcmp(MORSE_TABLE[i].pattern, pattern) == 0)
            return MORSE_TABLE[i].letter;
    }
    return '?';
}

static void led_feedback_blink(uint8_t times) {
    uint8_t saved = g_led_pattern;
    for (uint8_t i = 0; i < times; i++) {
        leds_set(0b111);    
        sleep_ms(60);
        leds_set(0);        
        sleep_ms(60);
    }
    leds_set(saved);        
}

static void handle_dot(void) {
    if (g_morse_idx < sizeof(g_morse_buf) - 1) {
        g_morse_buf[g_morse_idx++] = '.';
        g_morse_buf[g_morse_idx]   = '\0';
    }
    led_feedback_blink(1);  
    buzzer_beep_dot();
    printf("[MORSE] DOT   | buffer: %s\n", g_morse_buf);
}

static void handle_dash(void) {
    if (g_morse_idx < sizeof(g_morse_buf) - 1) {
        g_morse_buf[g_morse_idx++] = '-';
        g_morse_buf[g_morse_idx] = '\0';
    }
    led_feedback_blink(2);  
    buzzer_beep_dash();
    printf("[MORSE] DASH  | buffer: %s\n", g_morse_buf);
}

static void handle_letter_gap(void) {
    if (g_morse_idx == 0) return;       

    char letter = morse_decode(g_morse_buf);
    if (letter != '?' && g_word_idx < sizeof(g_decoded) - 1) {
        g_decoded[g_word_idx++] = letter;
        g_decoded[g_word_idx]   = '\0';
        printf("[MORSE] LETTER '%c' | word: %s\n", letter, g_decoded);
    } else {
        printf("[MORSE] UNKNOWN pattern: %s\n", g_morse_buf);
        buzzer_tone(300, 50);   
    }

    g_morse_idx = 0;
    g_morse_buf[0] = '\0';
}

static void handle_submit(void) {
    handle_letter_gap();   
    if (g_word_idx == 0) {
        printf("[GAME]  Empty submission ignored.\n");
        return; 
    }             
    printf("[GAME]  Submitted: \"%s\"\n", g_decoded);
    g_state = STATE_CHECKING;
}

// UART RX
static bool debug_led_state = false;

static void process_uart_rx(void) {
    while (uart_is_readable(UART_PORT)) {
        char c = uart_getc(UART_PORT);

        debug_led_state = !debug_led_state;
        gpio_put(DEBUG_LED_PIN, debug_led_state);

        printf("[UART] Received: 0x%02X '%c'\n", c, c);

        if (g_state != STATE_AWAITING_INPUT) continue;

        switch (c) {
            case '.': handle_dot(); break;
            case '-': handle_dash(); break;
            case ' ': handle_letter_gap(); break;
            case '\n': handle_submit(); break;
            default: break;
        }
        g_last_input_us = time_us_64();
    }
}

// Button input
static void button_isr(uint gpio, uint32_t events) {
    if (gpio != BUTTON_PIN) return;
    if (g_state != STATE_AWAITING_INPUT) return;

    uint64_t now = time_us_64();

    if (events & GPIO_IRQ_EDGE_FALL) {
        g_btn_press_us = now;
        g_btn_held = true;
    }
    else if ((events & GPIO_IRQ_EDGE_RISE) && g_btn_held) {
        uint32_t held_ms = (uint32_t)((now - g_btn_press_us) / 1000);
        g_btn_held = false;

        if (held_ms <= DOT_MAX_MS)
            handle_dot();
        else
            handle_dash();

        g_last_input_us = now;
    }
}

// LED & Game Logic 
static void leds_set(uint8_t pattern) {
    gpio_put(LED1_PIN, (pattern >> 0) & 1);
    gpio_put(LED2_PIN, (pattern >> 1) & 1);
    gpio_put(LED3_PIN, (pattern >> 2) & 1);
}

static void game_reset_input(void) {
    g_morse_idx = 0;
    g_morse_buf[0] = '\0';
    g_word_idx = 0;
    g_decoded[0] = '\0';
    g_last_input_us = time_us_64();
}

static void game_start(void) {
    g_strikes = 0;
    game_reset_input();

    g_led_pattern = rand() % 8;
    g_correct_word = WORD_BANK[g_led_pattern];
    leds_set(g_led_pattern);

    printf("\n[GAME] --- NEW ROUND --- \n");
    printf("[GAME]  LEDs : %d %d %d\n",(g_led_pattern >> 2) & 1,(g_led_pattern >> 1) & 1, (g_led_pattern >> 0) & 1);
    printf("[GAME]  Word : %s  (don't tell the defuser!)\n", g_correct_word);

    g_game_start_us = time_us_64();
    g_state = STATE_AWAITING_INPUT;

    uart_puts(UART_PORT, "READY\n");
}

static void game_check(void) {
    printf("[GAME]  Checking: got \"%s\", expected \"%s\"\n", g_decoded, g_correct_word);

    if (strcmp(g_decoded, g_correct_word) == 0) {
        g_state = STATE_DEFUSED;
        buzzer_victory();
        leds_set(0);
        uart_puts(UART_PORT, "DEFUSED\n");
        printf("[GAME] MODULE DEFUSED!!!\n");
    } else {
        g_strikes++;
        buzzer_strike();
        uart_puts(UART_PORT, "STRIKE\n");
        printf("[GAME] X STRIKE %d/3\n", g_strikes);

        if (g_strikes >= 3) {
            g_state = STATE_EXPLODED;
            leds_set(0b111);        
            buzzer_explode();
            uart_puts(UART_PORT, "EXPLODED\n");
            printf("[GAME] XXX EXPLODED XXX\n");
        } else {
            game_reset_input();
            g_state = STATE_AWAITING_INPUT;
            printf("[GAME]  Try again...\n");
        }
    }
}

// init and main
static void init_all(void) {
    stdio_init_all();
    sleep_ms(3000);          

    gpio_init(LED1_PIN); gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_init(LED2_PIN); gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_init(LED3_PIN); gpio_set_dir(LED3_PIN, GPIO_OUT);
    leds_set(0);

    gpio_init(DEBUG_LED_PIN);
    gpio_set_dir(DEBUG_LED_PIN, GPIO_OUT);
    gpio_put(DEBUG_LED_PIN, 0);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(
        BUTTON_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &button_isr
    );

    uart_init(UART_PORT, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_fifo_enabled(UART_PORT, true);

    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice, false);

    printf("=== MORSE CODE BOMB MODULE ===\n");
    printf("RP2350 Proton | ECE 362\n");
    printf("Peripherals: UART1, PWM, GPIO IRQ, HW Timer\n\n");
}

int main(void) {
    init_all();
    srand(time_us_32());

    for (int i = 0; i < 3; i++) {
        leds_set(0b111);
        sleep_ms(100);
        leds_set(0);
        sleep_ms(100);
    }

    printf("UART1 TX=GP%d, RX=GP%d, baud=%d\n", UART_TX_PIN, UART_RX_PIN, UART_BAUD);
    printf("Waiting for ESP32S3 to boot...\n");
    sleep_ms(2500);
    game_start();

    while (1) {
        uint64_t now = time_us_64();

        process_uart_rx();

        switch (g_state) {
        case STATE_AWAITING_INPUT:
            if (g_morse_idx > 0 &&
                (now - g_last_input_us) > LETTER_TIMEOUT_US) {
                handle_letter_gap();
            }
            if (g_word_idx > 0 && g_morse_idx == 0 &&
                (now - g_last_input_us) > SUBMIT_TIMEOUT_US) {
                printf("[GAME]  Timeout — auto-submitting\n");
                handle_submit();
            }

            if ((now - g_game_start_us) > GAME_TIMEOUT_US) {
                printf("[GAME]  Time's up!\n");
                g_state = STATE_EXPLODED;
                buzzer_explode();
                uart_puts(UART_PORT, "EXPLODED\n");
            }
            break;

        case STATE_CHECKING:
            game_check();
            break;

        case STATE_DEFUSED:
        case STATE_EXPLODED:
            if (!gpio_get(BUTTON_PIN)) {    
                sleep_ms(200);              
                printf("[GAME]  Restarting...\n");
                game_start();
            }
            break;

        default:
            break;
        }

        sleep_ms(10);   
    }

    return 0;
}