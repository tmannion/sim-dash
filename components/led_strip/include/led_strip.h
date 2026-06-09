#pragma once

#include <stdint.h>

void led_strip_init(void);
void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);
void led_strip_refresh(void);
void led_strip_clear(void);