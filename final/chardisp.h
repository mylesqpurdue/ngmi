#pragma once
#include "hardware/spi.h"

// SPI0 pins — defined as const int in main.c
extern const int SPI_DISP_SCK;
extern const int SPI_DISP_CSn;
extern const int SPI_DISP_TX;

void init_chardisp_pins(void);
void cd_init(void);
void cd_display1(const char *str);
void cd_display2(const char *str);
void send_spi_cmd(spi_inst_t *spi, uint16_t value);
void send_spi_data(spi_inst_t *spi, uint16_t value);
