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

const int SPI_7SEG_SCK = 14;
const int SPI_7SEG_CSn = 13;
const int SPI_7SEG_TX  = 15;

const int SPI_DISP_SCK = 34;
const int SPI_DISP_CSn = 33;
const int SPI_DISP_TX  = 35;

//////////////////////////////////////////////////////////////////////////////

// When testing the LCD display
#define STEP2
// When testing the 7-segment timer display (GPIO 26 start, GPIO 21 reset)
// #define STEP3
// When testing the full bomb game integration
// #define STEP4

//////////////////////////////////////////////////////////////////////////////

int main(void)
{
    #ifdef STEP2
    simon_says_init();
    init_chardisp_pins();
    cd_init();

    simon_says_startup_animation();

    cd_display1("Simon Says!     ");
    cd_display2("Press to start  ");

    // Wait for any button press — human timing seeds the random sequence
    while (gpio_get(SS_BTN_RED) && gpio_get(SS_BTN_GREEN) &&
           gpio_get(SS_BTN_BLUE) && gpio_get(SS_BTN_YELLOW)) sleep_ms(1);
    while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
           !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) sleep_ms(10);
    sleep_ms(300);

    cd_display1("Watch carefully!");
    cd_display2("                ");
    simon_says_demo(6);

    cd_display1("Your turn!      ");
    cd_display2("Repeat sequence ");
    if (simon_says_collect_input(6)) {
        cd_display1("Correct!        ");
        cd_display2("Module defused! ");
        gpio_put(SS_LED_RED,    1);
        gpio_put(SS_LED_GREEN,  1);
        gpio_put(SS_LED_BLUE,   1);
        gpio_put(SS_LED_YELLOW, 1);
    } else {
        cd_display1("Wrong!          ");
        cd_display2("Strike!         ");
        for (int i = 0; i < 3; i++) {
            gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1);
            gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1);
            sleep_ms(200);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            sleep_ms(200);
        }
    }
    #endif

    #ifdef STEP3
    timer_module_init();
    timer_set_game_state(GAME_ARMED);
    for (;;) {
        timer_update();
    }
    #endif

    for (;;);
    return 0;
}
