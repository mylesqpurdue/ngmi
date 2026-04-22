#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "seg7.h"
#include "timer_module.h"
#include "simon_says.h"
#include "chardisp.h"

//////////////////////////////////////////////////////////////////////////////

const char* username = "adelcuvi";

const int SEG7_DMA_CHANNEL = 0;

// SPI1 7-seg pin assignments are defined in seg7.h and used internally by seg7.c
// SPI0 char display pins
const int SPI_DISP_SCK = 34;
const int SPI_DISP_CSn = 33;
const int SPI_DISP_TX  = 35;

//////////////////////////////////////////////////////////////////////////////

// When testing the LCD display + Simon Says only
// #define STEP2
// When testing the 7-segment timer display (GPIO 26 start, GPIO 21 reset)
// #define STEP3
// Full integration: Simon Says + Timer + LCD running together
#define STEP4

//////////////////////////////////////////////////////////////////////////////

int main(void)
{
    stdio_init_all();

    #ifdef STEP2
    simon_says_init();
    simon_says_startup_animation();   // run before char display so a hung SPI init can't block this

    init_chardisp_pins();
    cd_init();

    int strikes = 0;
    char strike_line[17];

    while (1) {
        simon_says_reset();

        cd_display1("Simon Says!     ");
        snprintf(strike_line, sizeof(strike_line), "Strikes: %d      ", strikes);
        cd_display2(strike_line);

        // Wait for any button press — human timing seeds the random sequence
        while (gpio_get(SS_BTN_RED) && gpio_get(SS_BTN_GREEN) &&
               gpio_get(SS_BTN_BLUE) && gpio_get(SS_BTN_YELLOW)) sleep_ms(1);
        while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
               !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) sleep_ms(10);
        sleep_ms(300);

        // ── Round 1: Regular Simon Says ──────────────────────────────────────
        cd_display1("- Round 1 -     ");
        cd_display2("Regular Round   ");
        sleep_ms(2500);

        cd_display1("Watch carefully!");
        cd_display2("                ");
        simon_says_demo(6);

        cd_display1("Your turn!      ");
        cd_display2("Repeat sequence ");
        if (!simon_says_collect_input(6)) {
            strikes++;
            snprintf(strike_line, sizeof(strike_line), "Strikes: %d      ", strikes);
            cd_display1("Wrong! Strike!  ");
            cd_display2(strike_line);
            for (int i = 0; i < 3; i++) {
                gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
                gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
                sleep_ms(200);
                gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
                gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
                sleep_ms(200);
            }
            sleep_ms(1500);
            continue;
        }

        cd_display1("Correct!        ");
        cd_display2("Round 1 cleared!");
        gpio_put(SS_LED_RED,    1); gpio_put(SS_LED_GREEN,  1);
        gpio_put(SS_LED_BLUE,   1); gpio_put(SS_LED_YELLOW, 1);
        sleep_ms(2000);
        gpio_put(SS_LED_RED,    0); gpio_put(SS_LED_GREEN,  0);
        gpio_put(SS_LED_BLUE,   0); gpio_put(SS_LED_YELLOW, 0);

        // ── Transition to Round 2 ────────────────────────────────────────────
        cd_display1("- Round 2 -     ");
        cd_display2("Color Round     ");
        sleep_ms(2000);
        cd_display1("Use your manual!");
        cd_display2("Press when ready");
        while (gpio_get(SS_BTN_RED) && gpio_get(SS_BTN_GREEN) &&
               gpio_get(SS_BTN_BLUE) && gpio_get(SS_BTN_YELLOW)) sleep_ms(1);
        while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
               !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) sleep_ms(10);
        sleep_ms(300);

        // ── Round 2: Color Round (manual lookup) ─────────────────────────────
        simon_says_generate(6);

        static const char code_letters[] = "ABCD";
        char code_line[17];
        snprintf(code_line, sizeof(code_line), "Code: %c         ", code_letters[simon_says_get_code()]);
        cd_display1(code_line);
        cd_display2("Find your code! ");
        sleep_ms(5000);

        cd_display1(code_line);
        cd_display2("Press mapped btn");
        if (simon_says_color_round(6)) {
            cd_display1("Correct!        ");
            cd_display2("Module defused! ");
            gpio_put(SS_LED_RED,    1); gpio_put(SS_LED_GREEN,  1);
            gpio_put(SS_LED_BLUE,   1); gpio_put(SS_LED_YELLOW, 1);
            for (;;);
        }

        strikes++;
        snprintf(strike_line, sizeof(strike_line), "Strikes: %d      ", strikes);
        cd_display1("Wrong! Strike!  ");
        cd_display2(strike_line);
        for (int i = 0; i < 3; i++) {
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            sleep_ms(200);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            sleep_ms(200);
        }
        sleep_ms(1500);
    }
    #endif

    #ifdef STEP3
    timer_module_init();
    timer_set_game_state(GAME_ARMED);
    for (;;) {
        timer_update();
    }
    #endif

    #ifdef STEP4
    // ── Init all hardware ────────────────────────────────────────────────────
    simon_says_init();
    simon_says_startup_animation();

    timer_module_init();
    timer_set_game_state(GAME_ARMED);   // show start time on 7-seg right away

    init_chardisp_pins();
    cd_init();

    int strikes = 0;
    char strike_line[17];
    bool module_solved = false;

    cd_display1("NGMI Bomb Game  ");
    cd_display2("Press to start! ");

    // Wait for any button press to begin
    while (gpio_get(SS_BTN_RED) && gpio_get(SS_BTN_GREEN) &&
           gpio_get(SS_BTN_BLUE) && gpio_get(SS_BTN_YELLOW)) {
        timer_update();
        sleep_ms(1);
    }
    while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
           !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) sleep_ms(10);
    sleep_ms(300);

    // ── Start the countdown ──────────────────────────────────────────────────
    timer_set_game_state(GAME_ACTIVE);

    // ── Round 1: Regular Simon Says ──────────────────────────────────────────
    cd_display1("- Round 1 -     ");
    cd_display2("Regular Round   ");
    sleep_ms(1500);

    while (!module_solved) {
        simon_says_reset();

        cd_display1("Watch carefully!");
        cd_display2("                ");
        simon_says_demo(6);

        cd_display1("Your turn!      ");
        cd_display2("Repeat sequence ");

        if (simon_says_collect_input(6)) {
            cd_display1("Correct!        ");
            cd_display2("Round 1 cleared!");
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            timer_update();
            sleep_ms(2000);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            break;
        }

        strikes++;
        snprintf(strike_line, sizeof(strike_line), "Strike! (%d/3)  ", strikes);
        cd_display1("Wrong!          ");
        cd_display2(strike_line);
        for (int i = 0; i < 3; i++) {
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            sleep_ms(200);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            sleep_ms(200);
        }
        sleep_ms(1000);

        if (timer_is_expired() || strikes >= 3) goto exploded;
    }

    if (timer_is_expired() || strikes >= 3) goto exploded;

    // ── Round 2: Color Round ─────────────────────────────────────────────────
    cd_display1("- Round 2 -     ");
    cd_display2("Color Round     ");
    sleep_ms(1500);

    simon_says_generate(6);
    static const char code_letters[] = "ABCD";
    char code_line[17];
    snprintf(code_line, sizeof(code_line), "Code: %c         ", code_letters[simon_says_get_code()]);
    cd_display1(code_line);
    cd_display2("Check your manual");
    for (int i = 0; i < 50; i++) { timer_update(); sleep_ms(100); }  // 5 s to look up code

    cd_display1(code_line);
    cd_display2("Press mapped btn");

    if (simon_says_color_round(6)) {
        module_solved = true;
        timer_set_game_state(GAME_DEFUSED);
        cd_display1("DEFUSED!        ");
        cd_display2("Great work!     ");
        for (int i = 0; i < 10; i++) {
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            timer_update(); sleep_ms(150);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            timer_update(); sleep_ms(150);
        }
        for (;;) timer_update();
    }

    strikes++;
    snprintf(strike_line, sizeof(strike_line), "Strike! (%d/3)  ", strikes);
    cd_display1("Wrong!          ");
    cd_display2(strike_line);
    for (int i = 0; i < 3; i++) {
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
        gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
        sleep_ms(200);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
        sleep_ms(200);
    }

exploded:
    timer_set_game_state(GAME_EXPLODED);
    cd_display1("BOOM! Game over ");
    snprintf(strike_line, sizeof(strike_line), "Strikes: %d/3   ", strikes);
    cd_display2(strike_line);
    for (;;) timer_update();

    #endif

    for (;;);
    return 0;
}
