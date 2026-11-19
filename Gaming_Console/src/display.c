#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/timer.h"

#define TFT_WIDTH       240
#define TFT_HEIGHT      320
#define DISPLAY_TIME_MS 50

uint16_t __attribute__((aligned(16)))
    framebuffer[TFT_HEIGHT][TFT_WIDTH];

#define TFT_SCK   34
#define TFT_MOSI  35
#define TFT_CS    37
#define TFT_DC    33
#define TFT_RST   32

void display_init_spi(void) {

    // 1. Initialize your SPI peripheral
    spi_init(spi0, 10000000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // 2. Configure SCK and MOSI for hardware SPI
    gpio_set_function(TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_MOSI, GPIO_FUNC_SPI);


    // 3. Initialize CS, DC, and RST as GPIO outputs
    gpio_init(TFT_CS);
    gpio_set_dir(TFT_CS, true);
    gpio_put(TFT_CS, 1);

    gpio_init(TFT_DC);
    gpio_set_dir(TFT_DC, true);
    gpio_put(TFT_DC, 1);

    gpio_init(TFT_RST);
    gpio_set_dir(TFT_RST, true);
    gpio_put(TFT_RST, 1);
}

void tft_write_command(uint8_t cmd) {

    // 1. Pull CS LOW to select the display
    gpio_put(TFT_CS, 0);

    // 2. Pull DC LOW to indicate a command
    gpio_put(TFT_DC, 0);

    // 3. Send the command byte using SPI
    spi_write_blocking(spi0, &cmd, 1);

    // 4. Pull CS HIGH to deselect the display
    gpio_put(TFT_CS, 1);

}

void tft_write_data(uint8_t data) {

    // 1. Select the TFT
    gpio_put(TFT_CS, 0);


    // 2. Set DC to data mode
    gpio_put(TFT_DC, 1);

    // 3. Send the byte over SPI
    spi_write_blocking(spi0, &data, 1);

    // 4. Deselect the TFT
    gpio_put(TFT_CS, 1);
}

void tft_reset(void) {

    // 1. Set RST HIGH
    gpio_put(TFT_RST, 1);

    // 2. Wait 5 milliseconds
    sleep_ms(5);

    // 3. Set RST LOW
    gpio_put(TFT_RST, 0);

    // 4. Wait 20 milliseconds
    sleep_ms(20);
    // 5. Set RST HIGH again
    gpio_put(TFT_RST, 1);

    // 6. Wait 120 milliseconds
    sleep_ms(120);
}

void tft_init(void) {

     // 1. Initialize SPI
    display_init_spi();

    // 2. Reset the TFT
    tft_reset();

    // 3. Exit sleep mode (SLPOUT)
    tft_write_command(0x11);

    // 4. Wait for the controller to wake up
    sleep_ms(120);

    // 5. Set pixel format to RGB565 (16-bit color)
    tft_write_command(0x3A);
    tft_write_data(0x55);

    // 6. Turn the display on (DISPON)
    tft_write_command(0x29);

    // 7. Allow the display to stabilize
    sleep_ms(20);

}

void tft_set_window(uint16_t x0, uint16_t y0,
                    uint16_t x1, uint16_t y1) {

    // Set column addresses
    tft_write_command(0x2A);

    tft_write_data(x0 >> 8);
    tft_write_data(x0 & 0xFF);
    tft_write_data(x1 >> 8);
    tft_write_data(x1 & 0xFF);

    // Set row addresses
    tft_write_command(0x2B);

    tft_write_data(y0 >> 8);
    tft_write_data(y0 & 0xFF);
    tft_write_data(y1 >> 8);
    tft_write_data(y1 & 0xFF);
}

void tft_fill_screen(uint16_t color) {

    // Select the entire display
    tft_set_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    // Begin writing pixel data
    tft_write_command(0x2C);

    // Select TFT
    gpio_put(TFT_CS, 0);

    // Data mode
    gpio_put(TFT_DC, 1);

    // Send every pixel
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {

        uint8_t pixel[2] = {
            color >> 8,
            color & 0xFF
        };

        spi_write_blocking(spi0, pixel, 2);
    }

    // Deselect TFT
    gpio_put(TFT_CS, 1);
}





