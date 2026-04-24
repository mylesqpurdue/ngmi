#include "audio.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <math.h>

// ── Wavetable DDS (same approach as lab-5) ──────────────────────────────────
#define WAVETABLE_N   1000
#define SAMPLE_RATE   20000   // 20 kHz PWM sample rate

static int16_t wavetable[WAVETABLE_N];

// DDS accumulators (16.16 fixed-point)
static volatile int audio_step   = 0;
static volatile int audio_offset = 0;

// PWM slice/channel for GP36
static uint audio_slice;
static uint audio_chan;

// ── Beep timing (plays during WAVE_PLAYING) ─────────────────────────────────
#define BEEP_FREQ_HZ      500
#define BEEP_DURATION_MS   50
#define BEEP_INTERVAL_MS 1000

// ── Boom timing (plays once on entering WAVE_COMPLETE) ──────────────────────
#define BOOM_DURATION_MS 1500
#define BOOM_FREQ_START    80
#define BOOM_FREQ_END      40

// ── Internal state ──────────────────────────────────────────────────────────
static wave_state_t prev_state = WAVE_IDLE;

// Beep state
static uint32_t time_since_last_beep = 0;
static uint32_t beep_elapsed_ms = 0;
static bool     beep_tone_on = false;

// Boom state
static bool     boom_playing = false;
static uint32_t boom_elapsed_ms = 0;

// ── Wavetable init ──────────────────────────────────────────────────────────

static void init_wavetable(void) {
    for (int i = 0; i < WAVETABLE_N; i++) {
        wavetable[i] = (int16_t)(32767.0f * sinf(2.0f * (float)M_PI * (float)i / (float)WAVETABLE_N));
    }
}

// ── Set tone frequency via DDS step ─────────────────────────────────────────

static void set_freq(float freq_hz) {
    if (freq_hz <= 0.0f) {
        audio_step = 0;
        return;
    }
    // step = freq * N * 65536 / SAMPLE_RATE
    audio_step = (int)(freq_hz * (float)WAVETABLE_N * 65536.0f / (float)SAMPLE_RATE);
}

static void silence(void) {
    audio_step = 0;
    audio_offset = 0;
}

// ── PWM wrap ISR — runs at SAMPLE_RATE Hz ───────────────────────────────────

static void audio_pwm_isr(void) {
    pwm_clear_irq(audio_slice);

    audio_offset += audio_step;
    if (audio_offset >= (WAVETABLE_N << 16)) {
        audio_offset -= (WAVETABLE_N << 16);
    }

    if (audio_step == 0) {
        // Silent — set output to midpoint (no DC pop)
        uint32_t wrap = pwm_hw->slice[audio_slice].top;
        pwm_set_chan_level(audio_slice, audio_chan, wrap / 2);
        return;
    }

    // Read wavetable sample (-32767..+32767), scale to PWM range
    int16_t sample = wavetable[audio_offset >> 16];
    uint32_t wrap = pwm_hw->slice[audio_slice].top;
    // Map signed sample to 0..wrap range
    uint32_t level = (uint32_t)(((int32_t)sample + 32768) * (int32_t)wrap >> 16);
    pwm_set_chan_level(audio_slice, audio_chan, level);
}

// ── Init ────────────────────────────────────────────────────────────────────

void audio_init(void) {
    init_wavetable();

    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    audio_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    audio_chan  = pwm_gpio_to_channel(AUDIO_PIN);

    // Configure PWM for audio sample rate (same as lab-5)
    // clkdiv=150 divides 150 MHz → 1 MHz effective clock
    // wrap = 1000000 / 20000 = 50, so PWM fires at 20 kHz
    uint32_t divided_clk = 1000000;  // 150 MHz / 150
    uint32_t wrap = divided_clk / SAMPLE_RATE;

    pwm_set_clkdiv(audio_slice, 150.0f);
    pwm_set_wrap(audio_slice, wrap - 1);
    pwm_set_chan_level(audio_slice, audio_chan, 0);

    // Set up wrap interrupt
    pwm_clear_irq(audio_slice);
    pwm_set_irq_enabled(audio_slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, audio_pwm_isr);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    pwm_set_enabled(audio_slice, true);
    silence();
}

// ── Tick — call once per frame from main loop ───────────────────────────────

void audio_tick(wave_state_t state, uint32_t dt_ms) {
    bool state_changed = (state != prev_state);
    if (state_changed) {
        // Reset beep timers
        time_since_last_beep = 0;
        beep_elapsed_ms = 0;
        beep_tone_on = false;

        // Fire boom once on entering WAVE_COMPLETE
        if (state == WAVE_COMPLETE) {
            boom_playing = true;
            boom_elapsed_ms = 0;
        } else {
            boom_playing = false;
        }

        silence();
        prev_state = state;
    }

    // Boom has priority
    if (boom_playing) {
        boom_elapsed_ms += dt_ms;
        if (boom_elapsed_ms >= BOOM_DURATION_MS) {
            silence();
            boom_playing = false;
        } else {
            float t = (float)boom_elapsed_ms / (float)BOOM_DURATION_MS;
            float freq = (float)BOOM_FREQ_START -
                         t * (float)(BOOM_FREQ_START - BOOM_FREQ_END);
            set_freq(freq);
        }
        return;
    }

    // Beeping only during WAVE_PLAYING
    if (state == WAVE_PLAYING) {
        if (beep_tone_on) {
            beep_elapsed_ms += dt_ms;
            if (beep_elapsed_ms >= BEEP_DURATION_MS) {
                silence();
                beep_tone_on = false;
                beep_elapsed_ms = 0;
                time_since_last_beep = 0;
            }
        } else {
            time_since_last_beep += dt_ms;
            if (time_since_last_beep >= BEEP_INTERVAL_MS) {
                set_freq((float)BEEP_FREQ_HZ);
                beep_tone_on = true;
                beep_elapsed_ms = 0;
            }
        }
        return;
    }

    // All other states: silent
    silence();
}
