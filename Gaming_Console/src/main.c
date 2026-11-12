#include "pico/stdlib.h"

void tft_init(void);
void tft_fill_screen(uint16_t color);

int main(void) {

    tft_init();

    while (true) {

        tft_fill_screen(0xF800);  // Red
        sleep_ms(1000);

        tft_fill_screen(0x07E0);  // Green
        sleep_ms(1000);

        tft_fill_screen(0x001F);  // Blue
        sleep_ms(1000);
    }
}