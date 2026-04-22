#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "timer.h"
#include "button.h"
// #include "mfrc522.h"
#include "chardisp.h"
#include "simon_says.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"


const int SPI_DISP_SCK = 30;
const int SPI_DISP_CSn = 29;
const int SPI_DISP_TX  = 31;

#define RESET_PIN 17
#define ALARM_NUM 0

#define SERVO_PIN  0

//servo
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US    500   // Pulse width for 0°
#define SERVO_MAX_US    2500  // Pulse width for 180°
#define SERVO_RANGE_DEG 180

// //rfid
// #define MFRC522_SPI spi0
// #define PIN_MISO 4
// #define PIN_CS   5
// #define PIN_SCK  6
// #define PIN_MOSI 7
// #define PIN_RST  8

static uint servo_slice;
static uint servo_channel;



void init_sevenseg_spi();
void init_sevenseg_dma();
void sevenseg_display(const char* str);
void run_simon_says();

struct repeating_timer blink_timer;
volatile bool is_blinking = false;
volatile bool blink_on = true;
volatile bool current_r = false, current_g = false, current_b = false;
volatile uint64_t button_press_time = 0;

// Serial code display mode
volatile bool display_serial_code = false;
char serial_code[4] = "AAA"; // 3 characters + null terminator
char letters[9] = {'A', 'C', 'E', 'F', 'H', 'J', 'L', 'P', 'U'};
int parity = 0; // 0 for even, 1 for odd
int strike_count = 0; // Count of strikes
volatile bool module_complete = false; // Module complete flag

extern char font[];
bool is_at_180 = false;

// static void rfid_spi_init(void) {
//     // 1 MHz is a stable speed for the RC522
//     spi_init(MFRC522_SPI, 1000 * 1000);
//
//     // Assign SPI functions to the pins
//     gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
//     gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
//     gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
//
//     // Initialize Chip Select (CS/SDA) as a standard GPIO output
//     gpio_init(PIN_CS);
//     gpio_set_dir(PIN_CS, GPIO_OUT);
//     gpio_put(PIN_CS, 1); // Deselect chip by default (active low)
//
//     // Initialize Reset pin
//     gpio_init(PIN_RST);
//     gpio_set_dir(PIN_RST, GPIO_OUT);
//     gpio_put(PIN_RST, 1); // Take out of reset
// }

static void servo_init(void) {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    servo_slice   = pwm_gpio_to_slice_num(SERVO_PIN);
    servo_channel = pwm_gpio_to_channel(SERVO_PIN);
    
    // Set clock divider so 1 tick = 1 microsecond (assuming 125MHz sys_clk)
    pwm_set_clkdiv(servo_slice, 125.0f);
    // Wrap at 20,000 ticks for a 20ms period (50Hz)
    pwm_set_wrap(servo_slice, SERVO_PERIOD_US - 1);
    pwm_set_enabled(servo_slice, true);
}

static void servo_set_angle(uint16_t angle_deg) {
    if (angle_deg > SERVO_RANGE_DEG) angle_deg = SERVO_RANGE_DEG;
    
    uint16_t pulse_us = SERVO_MIN_US + 
        (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle_deg / SERVO_RANGE_DEG;
        
    pwm_set_chan_level(servo_slice, servo_channel, pulse_us);
}

void generate_serial_code() {
    uint32_t rand_val = get_rand_32();
    
    // First letter (A-Z)
    serial_code[0] = letters[rand_val % 9];
    rand_val = get_rand_32();
    
    // Second letter (A-Z)
    serial_code[1] = letters[rand_val % 9];
    rand_val = get_rand_32();
    
    // Number (0-9)
    serial_code[2] = '0' + (rand_val % 10);
    parity = (int)serial_code[2] % 2;
    serial_code[3] = '\0';
}

void irq_callback(uint gpio, uint32_t events) {
    if (gpio == RESET_PIN && (events & GPIO_IRQ_EDGE_FALL)){
        reset_isr();
    }
    if(gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)){
        button_isr();
    }
    if (gpio == SS_BTN_RED && (events & GPIO_IRQ_EDGE_RISE)){
        run_simon_says();
    }

    busy_wait_ms(500); // Debounce delay
}

void reset_isr() {

    countdown_secs = 300;
    timer_active = true;
    update_display = true;
    generate_serial_code();
    strike_count = 0; // Reset strikes on reset
    module_complete = false; // Reset completion flag
    gpio_put(CORRECT_LED, 0);
    printf("RESET pressed - starting Simon Says\n");
    // Stop any existing blink timer
    cancel_repeating_timer(&blink_timer);
    //simon says
    printf("Calling simon_says_startup_animation\n");
    simon_says_startup_animation();
    printf("Animation complete\n");

    printf("Displaying on char display\n");
    cd_display1("Simon Says!     ");
    cd_display2("Press to start  ");
    printf("Char display done\n");

    // 1. Pick a random color
    uint32_t button_type = get_rand_32() % 10;
    current_r = (button_type % 5 == 0 || button_type % 5 == 3 || button_type % 5 == 4); //red, green, blue, yellow, white
    current_g = (button_type % 5 == 1 || button_type % 5 == 3 || button_type % 5 == 4);
    current_b = (button_type % 5 == 2 || button_type % 5 == 4);

    // 2. Pick a random mode
    is_blinking = button_type / 5;

    // servo_set_angle(180);
    // is_at_180 = true;
    // sleep_ms(100); // Stabilize after servo movement

    if (is_blinking) {
        blink_on = true;
        apply_led_state(true);
        // Start the separate LED timer
        add_repeating_timer_ms(BLINK_INTERVAL_MS, led_blink_callback, NULL, &blink_timer);
    } else {
        // Solid mode: just turn it on and leave it
        apply_led_state(true);
    }

}
void run_simon_says(){
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
            busy_wait_ms(200);
            gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
            gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
            busy_wait_ms(200);
        }
    }
}

void reset_init(){
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_IN);
    gpio_disable_pulls(RESET_PIN);
    gpio_set_irq_enabled_with_callback(RESET_PIN, GPIO_IRQ_EDGE_FALL, true, &irq_callback);
}

int main() {
    stdio_init_all();
    //sleep_ms(2000);



    init_sevenseg_spi();
    init_sevenseg_dma();
    reset_init();
    button_init();
    rgb_init();
    // servo_init();
    // rfid_spi_init();
    simon_says_init();
    init_chardisp_pins();
    cd_init();

    init_hardware_timer(); 

    char display_buffer[9]; 

    // servo_set_angle(0);
    // MFRC522Ptr_t mfrc = MFRC522_Init(); 
    // PCD_Init(mfrc, spi0);
    // bool is_open = false;

    for(;;) {
        // if (PICC_IsNewCardPresent(mfrc)) {
        //     // Attempt to read the card serial
        //     if (PICC_ReadCardSerial(mfrc)) {
        //         printf("Successful Scan! Tag ID: ");
        //         for (uint8_t i = 0; i < mfrc->uid.size; i++) {
        //             printf("%02X", mfrc->uid.uidByte[i]);
        //         }
        //         printf("\n");

        //         // Toggle Servo
        //         if (is_open) {
        //             servo_set_angle(0);
        //             printf("Servo reset to 0 degrees\n");
        //         } else {
        //             servo_set_angle(180);
        //             printf("Servo turned to 180 degrees\n");
        //         }
        //         is_open = !is_open;

        //         // Delay to avoid multiple triggers from one tap
        //         sleep_ms(2000);
        //         printf("Waiting for next scan...\n");
        //     }
        // }
        if (update_display) {
            int mins = countdown_secs / 60;
            int secs = countdown_secs % 60;

            snprintf(display_buffer, sizeof(display_buffer), "%s%2d-%02d", serial_code, mins, secs);
            sevenseg_display(display_buffer);
            update_display = false;
        }
        sleep_ms(100);
        
    }

    return 0;

}
