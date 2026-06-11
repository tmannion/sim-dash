#pragma once

#include <stdint.h>

// Maximum number of LEDs this driver supports.
// For testing we use 1 (the onboard LED).
// Change to your strip length when the hardware arrives.
#define LED_STRIP_LED_COUNT     1
#define LED_STRIP_GPIO_PIN      8
#define LED_STRIP_RMT_RES_HZ    10000000  // 10MHz resolution, 1 tick = 100ns

// Initialise the RMT peripheral and encoder.
// Must be called once before any other led_strip function.
void led_strip_init(void);

// Set the colour of a single pixel in the internal buffer.
// Does NOT update the physical LED — call led_strip_refresh() after.
// index: 0-based LED index
// r, g, b: colour components 0–255
void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

// Transmit the current pixel buffer to the LED strip via RMT.
void led_strip_refresh(void);

// Set all pixels to off in the buffer and refresh.
void led_strip_clear(void);