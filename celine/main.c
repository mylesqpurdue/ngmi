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
// #define MFRC522_SPI spi1
// #define PIN_MISO 40
// #define PIN_CS   41
// #define PIN_SCK  42
// #define PIN_MOSI 43
// #define PIN_RST  44

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


void simon_says_init(void) {
    gpio_init(SS_LED_RED);    gpio_set_dir(SS_LED_RED,    GPIO_OUT); gpio_put(SS_LED_RED,    0);
    gpio_init(SS_LED_GREEN);  gpio_set_dir(SS_LED_GREEN,  GPIO_OUT); gpio_put(SS_LED_GREEN,  0);
    gpio_init(SS_LED_BLUE);   gpio_set_dir(SS_LED_BLUE,   GPIO_OUT); gpio_put(SS_LED_BLUE,   0);
    gpio_init(SS_LED_YELLOW); gpio_set_dir(SS_LED_YELLOW, GPIO_OUT); gpio_put(SS_LED_YELLOW, 0);

    gpio_init(SS_BTN_RED);    gpio_set_dir(SS_BTN_RED,    GPIO_IN); gpio_pull_up(SS_BTN_RED);
    gpio_init(SS_BTN_GREEN);  gpio_set_dir(SS_BTN_GREEN,  GPIO_IN); gpio_pull_up(SS_BTN_GREEN);
    gpio_init(SS_BTN_BLUE);   gpio_set_dir(SS_BTN_BLUE,   GPIO_IN); gpio_pull_up(SS_BTN_BLUE);
    gpio_init(SS_BTN_YELLOW); gpio_set_dir(SS_BTN_YELLOW, GPIO_IN); gpio_pull_up(SS_BTN_YELLOW);
    gpio_set_irq_enabled_with_callback(SS_BTN_RED, GPIO_IRQ_EDGE_FALL, true, &irq_callback);
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
    if (gpio == SS_BTN_RED && (events & GPIO_IRQ_EDGE_FALL)){
        printf("ss pressed\n");
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

    //init_chardisp_pins();

    init_sevenseg_spi();
    init_sevenseg_dma();
    //cd_init();
    reset_init();
    button_init();
    rgb_init();
    //servo_init();
    //simon_says_init();


    init_hardware_timer(); 

    char display_buffer[16];

    // // servo_set_angle(0);
    // MFRC522Ptr_t mfrc = MFRC522_Init(); 
    // PCD_Init(mfrc, MFRC522_SPI);
        
    // bool is_open = false;

    for(;;) {
        if (update_display) {
            int mins = countdown_secs / 60;
            int secs = countdown_secs % 60;

            snprintf(display_buffer, sizeof(display_buffer), "%s%2d-%02d", serial_code, mins, secs);
            sevenseg_display(display_buffer);
            update_display = false;
        }
        // if (PICC_IsNewCardPresent(mfrc)) {
        //     printf("Card detected!\n");
        //     // Attempt to read the card serial
        //     if (PICC_ReadCardSerial(mfrc)) {
        //         printf("RFID Card Scanned! Tag ID: ");
        //         for (uint8_t i = 0; i < mfrc->uid.size; i++) {
        //             printf("%02X", mfrc->uid.uidByte[i]);
        //         }
        //         printf("\n");

        //         // Delay to avoid multiple triggers from one tap
        //         sleep_ms(2000);
        //         printf("Ready for next scan...\n");
        //     } else {
        //         printf("Failed to read card serial\n");
        //     } 
        // }
        sleep_ms(100);
        
    }

    return 0;

}