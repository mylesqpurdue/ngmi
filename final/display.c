#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

const int SPI_7SEG_SCK = 14;
const int SPI_7SEG_CSn = 13;
const int SPI_7SEG_TX  = 15;
extern char font[];

uint16_t __attribute__((aligned(16))) message[8] = {
    (0 << 8) | 0x3F,
    (1 << 8) | 0x06,
    (2 << 8) | 0x5B,
    (3 << 8) | 0x4F,
    (4 << 8) | 0x66,
    (5 << 8) | 0x6D,
    (6 << 8) | 0x7D,
    (7 << 8) | 0x07,
};

void sevenseg_display(const char* str) {
    for (int i = 0; i < 8; i++) {
        if (str[i] != '\0') {
            message[i] = (i << 8) | font[(unsigned char)str[i]];
        } else {
            message[i] = 0;
        }
    }
}

void init_sevenseg_spi() {
    spi_init(spi1, 125000);

    gpio_init(SPI_7SEG_CSn);
    gpio_init(SPI_7SEG_SCK);
    gpio_init(SPI_7SEG_TX);
    gpio_set_function(SPI_7SEG_CSn, GPIO_FUNC_SPI);
    gpio_set_function(SPI_7SEG_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_7SEG_TX, GPIO_FUNC_SPI);

    spi_set_format(spi1, 16, 0, 0, SPI_MSB_FIRST);
}

void init_sevenseg_dma() {
    dma_hw->ch[1].read_addr = (uintptr_t)message;
    dma_hw->ch[1].write_addr = (uintptr_t)&spi1_hw->dr;
    dma_hw->ch[1].transfer_count = (DMA_CH1_TRANS_COUNT_MODE_VALUE_TRIGGER_SELF << DMA_CH1_TRANS_COUNT_MODE_LSB) | (8 << DMA_CH1_TRANS_COUNT_COUNT_LSB);

    uint32_t temp = 0;
    temp |= DMA_CH1_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_HALFWORD << DMA_CH1_CTRL_TRIG_DATA_SIZE_LSB;
    temp |= 1 << DMA_CH1_CTRL_TRIG_INCR_READ_LSB;
    temp |= 0 << DMA_CH1_CTRL_TRIG_INCR_WRITE_LSB;
    temp |= 4 << DMA_CH1_CTRL_TRIG_RING_SIZE_LSB;
    temp |= DREQ_SPI1_TX << DMA_CH1_CTRL_TRIG_TREQ_SEL_LSB;
    temp |= 1 << DMA_CH1_CTRL_TRIG_EN_LSB;

    dma_hw->ch[1].ctrl_trig = temp;
}
