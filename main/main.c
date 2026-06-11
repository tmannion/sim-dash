#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

void app_main(void)
{
    led_strip_init();
    led_strip_clear();

    while (1) {
        // Red for 1 second
        led_strip_set_pixel(0, 255, 0, 0);
        led_strip_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Green for 1 second
        led_strip_set_pixel(0, 0, 255, 0);
        led_strip_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Blue for 1 second
        led_strip_set_pixel(0, 0, 0, 255);
        led_strip_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Off for half a second
        led_strip_clear();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}