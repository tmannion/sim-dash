#include "led_strip.h"
#include <stdio.h>

void led_strip_init(void) {
    // TODO: initialise RMT peripheral
}

void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    // TODO: write pixel colour to buffer
}

void led_strip_refresh(void) {
    // TODO: transmit buffer to strip via RMT
}

void led_strip_clear(void) {
    // TODO: set all pixels to off
}