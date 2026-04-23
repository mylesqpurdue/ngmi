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

    spi_init(spi0, 10000);
    spi_set_format(spi0, 9, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
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
    send_spi_cmd(spi0, 0x38);
    sleep_us(40);
    send_spi_cmd(spi0, 0x0C);
    sleep_us(40);
    send_spi_cmd(spi0, 0x01);
    sleep_ms(2);
    send_spi_cmd(spi0, 0x06);
    sleep_us(40);
}

void cd_display1(const char *str) {
    send_spi_cmd(spi0, 0x80);
    for (int i = 0; i < 16; i++)
        send_spi_data(spi0, (uint16_t)str[i]);
}

void cd_display2(const char *str) {
    send_spi_cmd(spi0, 0xC0);
    for (int i = 0; i < 16; i++)
        send_spi_data(spi0, (uint16_t)str[i]);
}
