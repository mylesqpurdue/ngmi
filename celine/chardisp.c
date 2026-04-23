#include "chardisp.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

extern const int SPI_DISP_SCK;
extern const int SPI_DISP_CSn;
extern const int SPI_DISP_TX;

void init_chardisp_pins(void) {
    gpio_set_function(SPI_DISP_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_CSn, GPIO_FUNC_SPI);
    gpio_set_function(SPI_DISP_TX,  GPIO_FUNC_SPI);

    spi_init(spi1, 10000);
    spi_set_format(spi1, 9, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void send_spi_cmd(spi_inst_t *spi, uint16_t value) {
    while (spi_get_hw(spi)->sr & SPI_SSPSR_BSY_BITS)
        tight_loop_contents();
    spi_get_hw(spi)->dr = value;
}

void send_spi_data(spi_inst_t *spi, uint16_t value) {
    send_spi_cmd(spi, value | 0x100);
}

void cd_init(void) {
    sleep_ms(1);
    send_spi_cmd(spi1, 0x38);   // function set
    sleep_us(40);
    send_spi_cmd(spi1, 0x0C);   // display on, cursor off
    sleep_us(40);
    send_spi_cmd(spi1, 0x01);   // clear screen
    sleep_ms(2);
    send_spi_cmd(spi1, 0x06);   // entry mode: cursor moves right
    sleep_us(40);
}

void cd_display1(const char *str) {
    send_spi_cmd(spi1, 0x80);   // DDRAM address 0x00 = line 1
    for (int i = 0; i < 16; i++)
        send_spi_data(spi1, (uint16_t)str[i]);
}

void cd_display2(const char *str) {
    send_spi_cmd(spi1, 0xC0);   // DDRAM address 0x40 = line 2
    for (int i = 0; i < 16; i++)
        send_spi_data(spi1, (uint16_t)str[i]);
}
