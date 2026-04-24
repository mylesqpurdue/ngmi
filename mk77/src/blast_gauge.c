// blast_gauge.c — Krish's Ring-of-Fire / Blast Gauge module, refactored to be
// non-blocking and tickable so it can share a main loop with Myles's wave game.
//
// Differences vs. original standalone blast_gauge.c:
//   • Pins moved (WS2812 → GP16, SERVO → GP20) to avoid Myles's TFT pins.
//   • Auto-start replaced with BG_IDLE + first-button-press to arm.
//   • All sleep_ms() replaced with deadline-checked state transitions.
//   • button_pressed() (which blocks until release) replaced with an edge-
//     detector polled once per frame from main.
//   • Round logic unchanged — same 3 rounds, same zone math, same colors.

#include "blast_gauge.h"

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

// LED ring config 
#define NUM_LEDS_TOTAL 24
#define NUM_LEDS_ACTIVE 12 // top semicircle
#define FIRST_LED 0

// Servo calibration 
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
#define SERVO_RANGE_DEG 180

// Game tuning 
#define NUM_ZONES 4
#define LEDS_PER_ZONE 3 // 12 active / 4 zones

#define SPEED_ROUND1_MS 12
#define SPEED_ROUND2_MS 8
#define SPEED_ROUND3_MS 5

#define ZONE_SHIFT_MS 400 // Round 3 zone rotation

#define FLASH_ON_MS 150
#define FLASH_OFF_MS 75
#define PASS_FLASHES 3 // correct -> 3 green flashes
#define STRIKE_FLASHES 2 // wrong -> 2 red flashes
#define INTER_ROUND_MS 500

// Colors (GRB packed) 
#define COLOR_RED 0x00FF0000
#define COLOR_ORANGE 0x40FF0000
#define COLOR_YELLOW 0x80FF0000
#define COLOR_GREEN 0xFF000000
#define COLOR_OFF 0x00000000
#define DIM(c) (((c) >> 2) & 0x3F3F3F00)

static const uint32_t ZONE_COLORS[NUM_ZONES] = {
    DIM(COLOR_GREEN), DIM(COLOR_YELLOW), DIM(COLOR_ORANGE), DIM(COLOR_RED)
};
static const char *ZONE_NAMES[NUM_ZONES] = { "GREEN", "YELLOW", "ORANGE", "RED" };

// WS2812 PIO driver (same as original) 
static const uint16_t ws2812_program_instructions[] = {
    0x6221, 0x1123, 0x1400, 0xa442,
};
static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

static PIO ws_pio;
static uint ws_sm;
static int ws_dma_chan;
static uint32_t led_buffer[NUM_LEDS_TOTAL];

static void ws2812_init(void) {
    ws_pio = pio0;
    ws_sm = pio_claim_unused_sm(ws_pio, true);
    uint offset = pio_add_program(ws_pio, &ws2812_program);

    pio_gpio_init(ws_pio, BG_WS2812_PIN);
    pio_sm_set_consecutive_pindirs(ws_pio, ws_sm, BG_WS2812_PIN, 1, true);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + ws2812_program.length - 1);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, BG_WS2812_PIN);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = clock_get_hz(clk_sys) / (800000.0f * 10.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(ws_pio, ws_sm, offset, &c);
    pio_sm_set_enabled(ws_pio, ws_sm, true);

    ws_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(ws_dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(ws_pio, ws_sm, true));

    dma_channel_configure(ws_dma_chan, &dc, &ws_pio->txf[ws_sm], led_buffer, NUM_LEDS_TOTAL, false);
    memset(led_buffer, 0, sizeof(led_buffer));
}

static inline void ws_set(uint8_t idx, uint32_t grb) {
    if (idx < NUM_LEDS_TOTAL) led_buffer[idx] = grb;
}

static void ws_show(void) {
    dma_channel_wait_for_finish_blocking(ws_dma_chan);
    dma_channel_set_read_addr(ws_dma_chan, led_buffer, true);
}

static void ws_clear(void) {
    memset(led_buffer, 0, sizeof(led_buffer));
    ws_show();
}

// Servo
static uint servo_slice, servo_channel;
static uint16_t g_servo_angle = 0;

static void servo_init(void) {
    gpio_set_function(BG_SERVO_PIN, GPIO_FUNC_PWM);
    servo_slice = pwm_gpio_to_slice_num(BG_SERVO_PIN);
    servo_channel = pwm_gpio_to_channel(BG_SERVO_PIN);
    pwm_set_clkdiv(servo_slice, 125.0f);
    pwm_set_wrap(servo_slice, SERVO_PERIOD_US - 1);
    pwm_set_enabled(servo_slice, true);
}

static void servo_set_angle(uint16_t angle_deg) {
    if (angle_deg > SERVO_RANGE_DEG) angle_deg = SERVO_RANGE_DEG;
    uint16_t pulse_us = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle_deg / SERVO_RANGE_DEG;
    pwm_set_chan_level(servo_slice, servo_channel, pulse_us);
    g_servo_angle = angle_deg;
}

static uint8_t angle_to_led(uint16_t angle_deg) {
    if (angle_deg > SERVO_RANGE_DEG) angle_deg = SERVO_RANGE_DEG;
    uint16_t mirrored = SERVO_RANGE_DEG - angle_deg;
    uint8_t led = (uint8_t)((uint32_t)mirrored * (NUM_LEDS_ACTIVE - 1) / SERVO_RANGE_DEG);
    if (led >= NUM_LEDS_ACTIVE) led = NUM_LEDS_ACTIVE - 1;
    return led;
}

static inline uint8_t led_to_zone(uint8_t led_idx) { return led_idx / LEDS_PER_ZONE; }

// Button (non-blocking edge detect with debounce)
#define BUTTON_DEBOUNCE_MS 40

static void button_init(void) {
    gpio_init(BG_BUTTON_PIN);
    gpio_set_dir(BG_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BG_BUTTON_PIN);
}

bool bg_button_poll(uint64_t now_us) {
    static bool last_level_low = false;
    static uint64_t last_change_us = 0;
    static bool stable_low = false;

    bool raw_low = !gpio_get(BG_BUTTON_PIN); // active-low

    if (raw_low != last_level_low) {
        last_level_low = raw_low;
        last_change_us = now_us;
        return false; // Still bouncing
    }

    // Level has held this value since last_change_us
    if ((now_us - last_change_us) < (uint64_t)BUTTON_DEBOUNCE_MS * 1000) {
        return false;
    }

    // Debounced — report a press on the transition to low
    if (raw_low && !stable_low) {
        stable_low = true;
        return true;
    }
    if (!raw_low) stable_low = false;
    return false;
}

// Dedicated RGB LED
static void bg_led_init(void) {
    gpio_set_function(BG_LED_R_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BG_LED_G_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BG_LED_B_PIN, GPIO_FUNC_PWM);

    uint slice_r = pwm_gpio_to_slice_num(BG_LED_R_PIN);
    uint slice_g = pwm_gpio_to_slice_num(BG_LED_G_PIN);
    uint slice_b = pwm_gpio_to_slice_num(BG_LED_B_PIN);

    pwm_set_wrap(slice_r, 255);
    pwm_set_wrap(slice_g, 255);
    pwm_set_wrap(slice_b, 255);

    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);

    pwm_set_gpio_level(BG_LED_R_PIN, 255);
    pwm_set_gpio_level(BG_LED_G_PIN, 255);
    pwm_set_gpio_level(BG_LED_B_PIN, 255);
}

static void bg_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t sr = (uint8_t)((r * 30) / 255);
    uint8_t sg = (uint8_t)((g * 30) / 255);
    uint8_t sb = (uint8_t)((b * 30) / 255);
    pwm_set_gpio_level(BG_LED_R_PIN, 255 - sr);
    pwm_set_gpio_level(BG_LED_G_PIN, 255 - sg);
    pwm_set_gpio_level(BG_LED_B_PIN, 255 - sb);
}

static void bg_update_target_led(const bg_ctx_t *ctx) {
    if (ctx->state == BG_ROUND1 || ctx->state == BG_ROUND3) {
        switch (ctx->target_zone) {
            case 0: bg_led_set_color(0, 255, 0); break;   // GREEN
            case 1: bg_led_set_color(255, 255, 0); break; // YELLOW
            case 2: bg_led_set_color(255, 128, 0); break; // ORANGE
            case 3: bg_led_set_color(255, 0, 0); break;   // RED
            default: bg_led_set_color(0, 0, 0); break;
        }
    } else {
        bg_led_set_color(0, 0, 0);
    }
}

// Rendering 
static void render_round1(void) {
    for (int z = 0; z < NUM_ZONES; z++) {
        for (int i = 0; i < LEDS_PER_ZONE; i++) {
            ws_set(FIRST_LED + z * LEDS_PER_ZONE + i, ZONE_COLORS[z]);
        }
    }
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) ws_set(FIRST_LED + i, COLOR_OFF);
    ws_show();
}

static void render_round2(uint8_t green_start) {
    // All red except a green cluster. Rendered "inverted" so the green sweeps
    // opposite to the servo direction (same behavior as original).
    for (int i = 0; i < NUM_LEDS_ACTIVE; i++) {
        uint8_t inverted = NUM_LEDS_ACTIVE - 1 - i;
        bool    in_green = (inverted >= green_start) && (inverted < green_start + LEDS_PER_ZONE);
        ws_set(FIRST_LED + i, in_green ? DIM(COLOR_GREEN) : DIM(COLOR_RED));
    }
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) ws_set(FIRST_LED + i, COLOR_OFF);
    ws_show();
}

static void render_round3(uint8_t offset) {
    for (int i = 0; i < NUM_LEDS_ACTIVE; i++) {
        uint8_t shifted = (i + NUM_LEDS_ACTIVE - offset) % NUM_LEDS_ACTIVE;
        uint8_t zone    = shifted / LEDS_PER_ZONE;
        ws_set(FIRST_LED + i, ZONE_COLORS[zone]);
    }
    for (int i = NUM_LEDS_ACTIVE; i < NUM_LEDS_TOTAL; i++) ws_set(FIRST_LED + i, COLOR_OFF);
    ws_show();
}

// Removed render_idle_breathing

static void apply_flash_frame(uint32_t color, bool on) {
    uint32_t c = on ? color : COLOR_OFF;
    for (int i = 0; i < NUM_LEDS_ACTIVE; i++) ws_set(FIRST_LED + i, c);
    ws_show();
}

// Round setup 
static void setup_round(bg_ctx_t *ctx, bg_state_t round) {
    ctx->state = round;
    ctx->sweeping = true;
    ctx->angle = 0;
    ctx->direction = 1;
    ctx->next_step_us = 0;
    servo_set_angle(0);

    switch (round) {
        case BG_ROUND1:
            ctx->target_zone = rand() % NUM_ZONES;
            ctx->sweep_delay_ms = SPEED_ROUND1_MS;
            render_round1();
            printf("[BG] Round 1 — target zone %s\n", ZONE_NAMES[ctx->target_zone]);
            break;
        case BG_ROUND2:
            ctx->r2_green_start = (rand() % NUM_ZONES) * LEDS_PER_ZONE;
            ctx->target_zone = ctx->r2_green_start / LEDS_PER_ZONE;
            ctx->sweep_delay_ms = SPEED_ROUND2_MS;
            render_round2(ctx->r2_green_start);
            printf("[BG] Round 2 — green LEDs %d-%d\n",
                   ctx->r2_green_start, ctx->r2_green_start + LEDS_PER_ZONE - 1);
            break;
        case BG_ROUND3:
            ctx->target_zone = rand() % NUM_ZONES;
            ctx->r3_offset = 0;
            ctx->r3_last_shift_us = time_us_64();
            ctx->sweep_delay_ms  = SPEED_ROUND3_MS;
            render_round3(0);
            printf("[BG] Round 3 — target zone %s\n", ZONE_NAMES[ctx->target_zone]);
            break;
        default: break;
    }
}

// State helpers
static void start_flash(bg_ctx_t *ctx, bg_state_t flash_state, uint8_t pulses, uint64_t now_us) {
    ctx->state = flash_state;
    ctx->flash_remaining = (pulses * 2) - 2;
    ctx->flash_on = true;
    apply_flash_frame(flash_state == BG_FLASH_PASS ? DIM(COLOR_GREEN) : DIM(COLOR_RED), true);
    ctx->flash_next_toggle_us = now_us + (uint64_t)FLASH_ON_MS * 1000;
}

static uint8_t player_zone_for_stop(const bg_ctx_t *ctx) {
    uint8_t led = angle_to_led(g_servo_angle);
    if (ctx->state == BG_ROUND3) {
        uint8_t shifted = (led + NUM_LEDS_ACTIVE - ctx->r3_offset) % NUM_LEDS_ACTIVE;
        return shifted / LEDS_PER_ZONE;
    } else if (ctx->state == BG_ROUND2) {
        uint8_t inverted = NUM_LEDS_ACTIVE - 1 - led;
        return led_to_zone(inverted);
    } else {
        return led_to_zone(led);
    }
}

static bg_state_t next_round_after(bg_state_t s) {
    if (s == BG_ROUND1) return BG_ROUND2;
    if (s == BG_ROUND2) return BG_ROUND3;
    return BG_DEFUSED;
}

// Public API implementations 
void bg_init(bg_ctx_t *ctx) {
    ws2812_init();
    servo_init();
    button_init();
    bg_led_init();
    srand(time_us_32());

    memset(ctx, 0, sizeof(*ctx));
    ctx->state = BG_IDLE;
    ctx->direction = 1;
    servo_set_angle(0);
    render_round1();
    printf("[BG] Initialized. Waiting for button press to arm.\n");
    bg_update_target_led(ctx);
}

void bg_reset(bg_ctx_t *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = BG_IDLE;
    ctx->direction = 1;
    servo_set_angle(0);
    render_round1();
    printf("[BG] Reset to IDLE.\n");
    bg_update_target_led(ctx);
}

int bg_update(bg_ctx_t *ctx, bool button_edge, uint64_t now_us) {
    int event = 0;

    switch (ctx->state) {

    case BG_IDLE:
        if (button_edge) {
            printf("[BG] Armed! Starting Round 1.\n");
            setup_round(ctx, BG_ROUND1);
        }
        break;

    case BG_ROUND1:
    case BG_ROUND2:
    case BG_ROUND3: {
        // Stop-and-check on button edge
        if (button_edge && ctx->sweeping) {
            ctx->sweeping = false;
            uint8_t pz = player_zone_for_stop(ctx);
            printf("[BG] Stopped. angle=%u led=%u zone=%s target=%s\n",
                   g_servo_angle, angle_to_led(g_servo_angle),
                   ZONE_NAMES[pz], ZONE_NAMES[ctx->target_zone]);

            if (pz == ctx->target_zone) {
                // Correct
                bg_state_t next = next_round_after(ctx->state);
                if (next == BG_DEFUSED) {
                    printf("[BG] DEFUSED ✓\n");
                    event = 2;
                    ctx->pending_next = BG_DEFUSED;
                } else {
                    ctx->pending_next = next;
                }
                start_flash(ctx, BG_FLASH_PASS, PASS_FLASHES, now_us);
            } else {
                ctx->strikes++;
                event = 1;
                printf("[BG] STRIKE (module-local count %u)\n", ctx->strikes);
                ctx->pending_next = ctx->state;
                start_flash(ctx, BG_FLASH_STRIKE, STRIKE_FLASHES, now_us);
            }
            break;
        }

        // Advance sweep on a deadline (no sleeps)
        if (ctx->sweeping && now_us >= ctx->next_step_us) {
            ctx->angle += ctx->direction;
            if (ctx->angle >= SERVO_RANGE_DEG) {
                ctx->angle = SERVO_RANGE_DEG;
                ctx->direction = -1;
            } else if (ctx->angle <= 0) {
                ctx->angle = 0;
                ctx->direction = 1;
            }
            servo_set_angle((uint16_t)ctx->angle);
            ctx->next_step_us = now_us + (uint64_t)ctx->sweep_delay_ms * 1000;

            // Round 3 zone rotation
            if (ctx->state == BG_ROUND3 &&
                (now_us - ctx->r3_last_shift_us) > (uint64_t)ZONE_SHIFT_MS * 1000) {
                ctx->r3_offset = (ctx->r3_offset + 1) % NUM_LEDS_ACTIVE;
                render_round3(ctx->r3_offset);
                ctx->r3_last_shift_us = now_us;
            }
        }
        break;
    }

    case BG_FLASH_PASS:
    case BG_FLASH_STRIKE: {
        if (now_us >= ctx->flash_next_toggle_us && ctx->flash_remaining > 0) {
            ctx->flash_on = !ctx->flash_on;
            uint32_t color = (ctx->state == BG_FLASH_PASS) ? DIM(COLOR_GREEN) : DIM(COLOR_RED);
            apply_flash_frame(color, ctx->flash_on);
            ctx->flash_remaining--;
            uint32_t ms = ctx->flash_on ? FLASH_ON_MS : FLASH_OFF_MS;
            ctx->flash_next_toggle_us = now_us + (uint64_t)ms * 1000;
        }
        if (ctx->flash_remaining == 0 && now_us >= ctx->flash_next_toggle_us) {
            // Flash done - resolve to the pending state
            bg_state_t next = ctx->pending_next;
            if (next == BG_DEFUSED) {
                ctx->state = BG_DEFUSED;
                render_round1();
            } else if (next == BG_EXPLODED) {
                ctx->state = BG_EXPLODED;
                ws_clear();
            } else {
                // INTER_ROUND -> next round (or re-enter same round after strike)
                ctx->state = BG_INTER_ROUND;
                ctx->hold_until_us = now_us + (uint64_t)INTER_ROUND_MS * 1000;
                ctx->pending_next  = next;
            }
        }
        break;
    }

    case BG_INTER_ROUND:
        if (now_us >= ctx->hold_until_us) {
            setup_round(ctx, ctx->pending_next);
        }
        break;

    case BG_DEFUSED:
    case BG_EXPLODED:
        // Terminal -> main.c decides when to reset
        break;
    }

    bg_update_target_led(ctx);
    return event;
}

// Accessors
const char *bg_state_name(bg_state_t s) {
    switch (s) {
        case BG_IDLE: return "IDLE";
        case BG_ROUND1: return "ROUND1";
        case BG_ROUND2: return "ROUND2";
        case BG_ROUND3: return "ROUND3";
        case BG_FLASH_PASS: return "PASS";
        case BG_FLASH_STRIKE: return "STRIKE";
        case BG_INTER_ROUND: return "WAIT";
        case BG_DEFUSED: return "DEFUSED";
        case BG_EXPLODED: return "EXPLODED";
    }
    return "?";
}

uint16_t bg_servo_angle(const bg_ctx_t *ctx) { (void)ctx; return g_servo_angle; }

uint8_t bg_current_round(const bg_ctx_t *ctx) {
    switch (ctx->state) {
        case BG_ROUND1: return 1;
        case BG_ROUND2: return 2;
        case BG_ROUND3: return 3;
        default: return 0;
    }
}