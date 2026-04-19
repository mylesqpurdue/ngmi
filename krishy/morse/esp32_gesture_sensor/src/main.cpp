// I ain't finish training the model yet 
// Updated code will come soon 
// Test code rn works for UART Communication 
// This old code is just a placeholder 
// Until I finish debugging my curr

#include <Arduino.h>

// Initialize Hardware UART on UART1
HardwareSerial RP_UART(1); 

#define RP2350_TXD_PIN 43  // D6 - Connects to RP2350 RX (GP9)
#define RP2350_RXD_PIN 44  // D7 - Connects to RP2350 TX (GP8)
#define UART_BAUD 115200

void setup() {
    Serial.begin(115200);
    RP_UART.begin(UART_BAUD, SERIAL_8N1, RP2350_RXD_PIN, RP2350_TXD_PIN);

    delay(2000); // Give USB serial time to connect
    Serial.println("Starting Morse Code Defusal ESP32 Module...");
    Serial.println("Type 'f' (dot), 'p' (dash), 'v' (gap), or 's' (submit) to test.");
}

void loop() {
    // 1. Read from the Arduino USB Serial Monitor and send to RP2350
    if (Serial.available()) {
        char c = Serial.read();
        char morse_char = 0;
        
        switch(c) {
            case 'f': morse_char = '.'; break;
            case 'p': morse_char = '-'; break;
            case 'v': morse_char = ' '; break;
            case 's': morse_char = '\n'; break;
        }

        if (morse_char != 0) {
            RP_UART.write(morse_char);
            Serial.printf("Sent: '%c'\n", morse_char);
        }
    }

    // 2. Read incoming status messages from the RP2350
    if (RP_UART.available()) {
        String incomingMessage = RP_UART.readStringUntil('\n');
        Serial.printf("RP2350 Status: %s\n", incomingMessage.c_str());
    }
}