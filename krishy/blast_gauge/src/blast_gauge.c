#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

// PIN ASSIGNMENTS
#define WS2812_PIN 14 // PIO output -> WS2812 data in
#define SERVO_PIN  15 // PWM -> servo signal
#define BUTTON_PIN 5 // Active-low, internal pull-up

//LED RING CONFIG
#define NUM_LEDS_TOTAL 24 // Full ring
#define NUM_LEDS_ACTIVE 12 // Top semicircle only
#define FIRST_LED 0 // First active LED index on the ring
                    // Adjust if ring's "top" starts elsewhere

// SERVO CALIBRATION
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 500 // Pulse at 0°
#define SERVO_MAX_US 2500 // Pulse at 180°
#define SERVO_RANGE_DEG 180

// GAME ZONES 
#define NUM_ZONES 4
#define LEDS_PER_ZONE 3 // 12 LEDs / 4 zones

// SWEEP SPEEDS (ms per degree step)
#define SPEED_ROUND1 18
#define SPEED_ROUND2 12
#define SPEED_ROUND3 8

// ZONE SHIFT INTERVAL (ms) for Round 3 
#define ZONE_SHIFT_MS 400

// COLORS (GRB order packed into uint32_t) 
#define COLOR_RED 0x00FF0000 // G=0x00, R=0xFF, B=0x00
#define COLOR_ORANGE 0x40FF0000 // G=0x40, R=0xFF, B=0x00
#define COLOR_YELLOW 0x80FF0000 // G=0x80, R=0xFF, B=0x00
#define COLOR_GREEN 0xFF000000 // G=0xFF, R=0x00, B=0x00
#define COLOR_OFF 0x00000000
#define COLOR_WHITE 0xFFFFFF00 // G=0xFF, R=0xFF, B=0xFF

// Dimmed versions for less blinding operation
#define DIM(c) (((c) >> 2) & 0x3F3F3F00)

static const uint32_t ZONE_COLORS[NUM_ZONES] = {
    DIM(COLOR_GREEN),
    DIM(COLOR_YELLOW),
    DIM(COLOR_ORANGE),
    DIM(COLOR_RED)
};

static const char *ZONE_NAMES[NUM_ZONES] = {
    "GREEN", "YELLOW", "ORANGE", "RED"
};

// GAME STATES
typedef enum {
    GAME_ROUND1,
    GAME_ROUND2,
    GAME_ROUND3,
    GAME_DEFUSED,
    GAME_EXPLODED,
    GAME_WAIT_NEXT_ROUND
} game_state_t;


// WS2812 DRIVER (PIO-based)
/* 
 *  Uses a pre-compiled PIO program to generate the precise
 *  800kHz timing WS2812 LEDs require. DMA pushes the pixel
 *  buffer automatically -- zero CPU overhead during transfer.
 * 
 */

/*
 * Pre-compiled WS2812 PIO program (from Pico SDK examples).
 * 4 instructions, uses side-set for the data pin.
 *
 * .program ws2812
 * .side_set 1
 *   out x, 1 -> side 0 [2]; shift 1 bit, drive low
 *   jmp !x, 3 -> side 1 [1]; drive high, branch on bit
 *   jmp 0 -> side 1 [4]; bit=1: stay high longer
 *   nop s -> ide 0 [4]; bit=0: go low
 */
static const uint16_t ws2812_program_instructions[] = {
    0x6221, // out x, 1 -> side 0 [2]
    0x1123, // jmp !x, 3 -> side 1 [1]
    0x1400, // jmp 0 -> side 1 [4]
    0xa442, // nop -> side 0 [4]
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

static PIO ws_pio;
static uint ws_sm;
static int ws_dma_chan;
static uint32_t led_buffer[NUM_LEDS_TOTAL]; // GRB pixel data

static void ws2812_init(void) {
    ws_pio = pio0;
    ws_sm = pio_claim_unused_sm(ws_pio, true);

    uint offset = pio_add_program(ws_pio, &ws2812_program);

    // Config the pin for PIO
    pio_gpio_init(ws_pio, WS2812_PIN);
    pio_sm_set_consecutive_pindirs(ws_pio, ws_sm, WS2812_PIN, 1, true);

    // Configure state machine
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + ws2812_program.length - 1);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, WS2812_PIN);
    sm_config_set_out_shift(&c, false, true, 24); // shift left, autopull, 24 bits
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Clock: 800kHz data rate × 10 cycles/bit = 8 MHz PIO clock
    float div = clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(ws_pio, ws_sm, offset, &c);
    pio_sm_set_enabled(ws_pio, ws_sm, true);

    // Set up DMA channel for automatic pixel transfer
    ws_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(ws_dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(ws_pio, ws_sm, true));

    dma_channel_configure(
        ws_dma_chan, &dc,
        &ws_pio->txf[ws_sm], // Write to PIO TX FIFO
        led_buffer, // Read from pixel buffer
        NUM_LEDS_TOTAL, // Transfer count
        false // Don't start yet
    );

    // Clear all LEDs
    memset(led_buffer, 0, sizeof(led_buffer));
}

// Set a single pixel (index 0–23). Color is 0xGGRRBB00 format.
static inline void ws2812_set_pixel(uint8_t index, uint32_t grb) {
    if (index < NUM_LEDS_TOTAL)
        led_buffer[index] = grb;
}

// Set pixel from separate R, G, B values (0–255 each). 
static inline void ws2812_set_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    ws2812_set_pixel(index, ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8));
}

// Push the pixel buffer to the LED ring via DMA. 
static void ws2812_show(void) {
    dma_channel_wait_for_finish_blocking(ws_dma_chan);
    dma_channel_set_read_addr(ws_dma_chan, led_buffer, true);
}

// Clr all LEDs
static void ws2812_clear(void) {
    memset(led_buffer, 0, sizeof(led_buffer));
    ws2812_show();
}

// SERVO CONTROL
static uint servo_slice;
static uint servo_channel;
static uint16_t g_servo_angle = 0;

static void servo_init(void) {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    servo_slice   = pwm_gpio_to_slice_num(SERVO_PIN);
    servo_channel = pwm_gpio_to_channel(SERVO_PIN);
    pwm_set_clkdiv(servo_slice, 125.0f);
    pwm_set_wrap(servo_slice, SERVO_PERIOD_US - 1);
    pwm_set_enabled(servo_slice, true);
}

static void servo_set_angle(uint16_t angle_deg) {
    if (angle_deg > SERVO_RANGE_DEG) angle_deg = SERVO_RANGE_DEG;
    uint16_t pulse_us = SERVO_MIN_US +
        (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle_deg / SERVO_RANGE_DEG;
    pwm_set_chan_level(servo_slice, servo_channel, pulse_us);
    g_servo_angle = angle_deg;
}

static uint8_t angle_to_led(uint16_t angle_deg) {
    // Safety clamp to prevent underflow
    if (angle_deg > SERVO_RANGE_DEG) angle_deg = SERVO_RANGE_DEG;
    
    // between the servo's sweep direction and the LED ring's data line direction.
    uint16_t mirrored_angle = SERVO_RANGE_DEG - angle_deg;
    
    uint8_t led = (uint16_t)((uint32_t)mirrored_angle * (NUM_LEDS_ACTIVE - 1) / SERVO_RANGE_DEG);
    if (led >= NUM_LEDS_ACTIVE) led = NUM_LEDS_ACTIVE - 1;
    return led;
}

static uint8_t led_to_zone(uint8_t led_index) {
    return led_index / LEDS_PER_ZONE;
}


// Button
static void button_init(void) {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
}

static bool button_pressed(void) {
    if (!gpio_get(BUTTON_PIN)) {
        sleep_ms(50);
        if (!gpio_get(BUTTON_PIN)) {
            while (!gpio_get(BUTTON_PIN));
            sleep_ms(50);
            return true;
        }
    }
    return false;
}

// LED RENDERING
/**
 * Render Round 1: static colored zones.
 * Zone 0 = LEDs 0-2, Zone 1 = LEDs 3-5, etc.
 */
static void render_round1(void) {
    for (int z = 0; z < NUM_ZONES; z++) {
        for (int i = 0; i < LEDS_PER_ZONE; i++) {
            uint8_t led = FIRST_LED + z * LEDS_PER_ZONE + i;
            ws2812_set_pixel(led, ZONE_COLORS[z]);
        }
    }
    // Turn off inactive LEDs (bottom half)
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) {
        ws2812_set_pixel(FIRST_LED + i, COLOR_OFF);
    }
    ws2812_show();
}

/**
 * Render Round 2: all red except one green cluster.
 * green_start = first LED of the green zone (0–9 for 3-LED cluster).
 */
static void render_round2(uint8_t green_start) {
    for (int i = 0; i < NUM_LEDS_ACTIVE; i++) {
        // Reverse the LED index so the green appears on the opposite side
        uint8_t led = FIRST_LED + (NUM_LEDS_ACTIVE - 1 - i);
        if (i >= green_start && i < green_start + LEDS_PER_ZONE) {
            ws2812_set_pixel(led, DIM(COLOR_GREEN));
        } else {
            ws2812_set_pixel(led, DIM(COLOR_RED));
        }
    }
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) {
        ws2812_set_pixel(FIRST_LED + i, COLOR_OFF);
    }
    ws2812_show();
}

/**
 * Render Round 3: all 4 zones shifted by offset.
 * offset rotates all zones around the ring.
 */
static void render_round3(uint8_t offset) {
    for (int i = 0; i < NUM_LEDS_ACTIVE; i++) {
        // Shifted zone index
        uint8_t shifted_pos = (i + NUM_LEDS_ACTIVE - offset) % NUM_LEDS_ACTIVE;
        uint8_t zone = shifted_pos / LEDS_PER_ZONE;
        uint8_t led  = FIRST_LED + i;
        ws2812_set_pixel(led, ZONE_COLORS[zone]);
    }
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) {
        ws2812_set_pixel(FIRST_LED + i, COLOR_OFF);
    }
    ws2812_show();
}

/**
 * Flash all active LEDs a color briefly (for feedback).
 */
static void flash_feedback(uint32_t color, uint16_t duration_ms, uint8_t times) {
    for (uint8_t t = 0; t < times; t++) {
        for (int i = 0; i < NUM_LEDS_ACTIVE; i++)
            ws2812_set_pixel(FIRST_LED + i, color);
        ws2812_show();
        sleep_ms(duration_ms);

        // Only clear between flashes, not on the last one
        if (t < times - 1) {
            ws2812_clear();
            sleep_ms(duration_ms / 2);
        }
    }
    // Wait for final DMA to complete before returning
    dma_channel_wait_for_finish_blocking(ws_dma_chan);
}

// GAME LOGIC
static game_state_t g_state = GAME_ROUND1;
static uint8_t g_strikes = 0;
static uint8_t g_target_zone = 0; // Which zone the player must stop in
static uint8_t g_r2_green_start = 0; // Round 2: green cluster start LED
static uint8_t g_r3_offset = 0; // Round 3: zone rotation offset
static uint64_t g_r3_last_shift = 0; // Round 3: last shift timestamp

// Sweep state
static int16_t g_angle = 0;
static int8_t g_direction = 1;
static uint16_t g_sweep_delay = SPEED_ROUND1;
static bool g_sweeping = true;

static void game_setup_round(void) {
    g_sweeping = true;
    g_angle = 0;
    g_direction = 1;
    servo_set_angle(0);

    switch (g_state) {
    case GAME_ROUND1:
        g_target_zone = rand() % NUM_ZONES;
        g_sweep_delay = SPEED_ROUND1;
        render_round1();
        printf("\n=== ROUND 1: ZONE STOP ===\n");
        printf("Zones: GREEN(0-2) YELLOW(3-5) ORANGE(6-8) RED(9-11)\n");
        printf("TARGET: %s (zone %d)\n\n", ZONE_NAMES[g_target_zone], g_target_zone);
        break;

    case GAME_ROUND2:
        // Random green cluster position (must start at zone boundary for clean zones)
        g_r2_green_start = (rand() % NUM_ZONES) * LEDS_PER_ZONE;
        g_target_zone = g_r2_green_start / LEDS_PER_ZONE;
        g_sweep_delay = SPEED_ROUND2;
        render_round2(g_r2_green_start);
        printf("\n=== ROUND 2: FIND THE GREEN ===\n");
        printf("Green zone at LEDs %d-%d\n", g_r2_green_start,
               g_r2_green_start + LEDS_PER_ZONE - 1);
        printf("Stop in the GREEN zone!\n\n");
        break;

    case GAME_ROUND3:
        g_target_zone = rand() % NUM_ZONES;
        g_r3_offset = 0;
        g_r3_last_shift = time_us_64();
        g_sweep_delay = SPEED_ROUND3;
        render_round3(0);
        printf("\n=== ROUND 3: SHIFTING ZONES ===\n");
        printf("All zones are moving! TARGET: %s\n", ZONE_NAMES[g_target_zone]);
        printf("Stop when the needle is in %s!\n\n", ZONE_NAMES[g_target_zone]);
        break;

    default:
        break;
    }
}

/**
 * Check if the player stopped in the correct zone.
 * For Round 3, the zone positions are shifted so we need to
 * account for the current offset.
 */
static void game_check_stop(void) {
    uint8_t led_index = angle_to_led(g_servo_angle);
    uint8_t player_zone;

    if (g_state == GAME_ROUND3) {
        // Account for zone shift: reverse the offset to find which
        // logical zone is currently at this LED position
        uint8_t shifted_pos = (led_index + NUM_LEDS_ACTIVE - g_r3_offset) % NUM_LEDS_ACTIVE;
        player_zone = shifted_pos / LEDS_PER_ZONE;
    } else if (g_state == GAME_ROUND2) {
        // Round 2 LEDs are rendered inverted, so invert the detection too
        uint8_t inverted_led = NUM_LEDS_ACTIVE - 1 - led_index;
        player_zone = led_to_zone(inverted_led);
    } else {
        player_zone = led_to_zone(led_index);
    }

    printf("[STOP] Angle: %d deg, LED: %d, Zone: %s\n",
           g_servo_angle, led_index, ZONE_NAMES[player_zone]);
    printf("[STOP] Target was: %s\n", ZONE_NAMES[g_target_zone]);

    if (player_zone == g_target_zone) {
        printf("[GAME] << CORRECT! >>\n");
        flash_feedback(DIM(COLOR_GREEN), 150, 3);

        // Advance to next round
        switch (g_state) {
        case GAME_ROUND1:
            g_state = GAME_ROUND2;
            break;
        case GAME_ROUND2:
            g_state = GAME_ROUND3;
            break;
        case GAME_ROUND3:
            g_state = GAME_DEFUSED;
            printf("\n*** MODULE DEFUSED ***\n");
            // Victory animation: green sweep
            for (int k = 0; k < 3; k++) {
                for (int i = 0; i < NUM_LEDS_ACTIVE; i++) {
                    ws2812_set_pixel(FIRST_LED + i, DIM(COLOR_GREEN));
                    ws2812_show();
                    sleep_ms(30);
                }
                ws2812_clear();
                sleep_ms(100);
            }
            return;
        default:
            break;
        }

        // Brief pause then start next round
        sleep_ms(500);
        game_setup_round();

    } else {
        // WRONG ZONE 
        g_strikes++;
        printf("[GAME] X STRIKE %d/3\n", g_strikes);
        flash_feedback(DIM(COLOR_RED), 200, 2);

        if (g_strikes >= 3) {
            g_state = GAME_EXPLODED;
            printf("\n!!! EXPLODED !!!\n");
            // Explosion animation: red flash
            for (int k = 0; k < 5; k++) {
                for (int i = 0; i < NUM_LEDS_ACTIVE; i++)
                    ws2812_set_pixel(FIRST_LED + i, DIM(COLOR_RED));
                ws2812_show();
                sleep_ms(80);
                ws2812_clear();
                sleep_ms(80);
            }
            return;
        }

        // Resume same round
        sleep_ms(500);
        g_sweeping = true;

        // Re-render current round
        switch (g_state) {
        case GAME_ROUND1: render_round1(); break;
        case GAME_ROUND2: render_round2(g_r2_green_start); break;
        case GAME_ROUND3: render_round3(g_r3_offset); break;
        default: break;
        }
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(500);

    ws2812_init();
    servo_init();
    button_init();

    srand(time_us_32());

    printf("===========================\n");
    printf("  RING OF FIRE — BOMB GAUGE\n");
    printf("  ECE 362 | RP2350 Proton\n");
    printf("===========================\n");
    printf("WS2812: GP%d | Servo: GP%d | Button: GP%d\n\n", WS2812_PIN, SERVO_PIN, BUTTON_PIN);

    // Start at 0° and begin round 1
    servo_set_angle(0);
    sleep_ms(500);
    game_setup_round();

    while (1) {
        if (g_state == GAME_DEFUSED || g_state == GAME_EXPLODED) {
            // Press button to restart
            if (button_pressed()) {
                g_state   = GAME_ROUND1;
                g_strikes = 0;
                printf("\n[GAME] Restarting...\n");
                game_setup_round();
            }
            sleep_ms(50);
            continue;
        }

        // Button press -> stop and check 
        if (button_pressed() && g_sweeping) {
            g_sweeping = false;
            game_check_stop();
            continue;
        }

        // Sweep the servo 
        if (g_sweeping) {
            g_angle += g_direction;

            if (g_angle >= SERVO_RANGE_DEG) {
                g_angle = SERVO_RANGE_DEG;
                g_direction = -1;
            } else if (g_angle <= 0) {
                g_angle = 0;
                g_direction = 1;
            }

            servo_set_angle((uint16_t)g_angle);

            // Round 3: shift zones periodically
            if (g_state == GAME_ROUND3) {
                uint64_t now = time_us_64();
                if ((now - g_r3_last_shift) > (uint64_t)ZONE_SHIFT_MS * 1000) {
                    g_r3_offset = (g_r3_offset + 1) % NUM_LEDS_ACTIVE;
                    render_round3(g_r3_offset);
                    g_r3_last_shift = now;
                }
            }

            sleep_ms(g_sweep_delay);
        } else {
            sleep_ms(10);
        }
    }

    return 0;
}