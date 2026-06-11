#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>

static const char *TAG = "led_strip";

// ----------------------------------------------------------------
// Internal encoder types and state
// ----------------------------------------------------------------

// This struct extends rmt_encoder_t with the extra state our
// custom encoder needs. The 'base' member MUST be first — this
// allows __containerof() to recover the full struct from a pointer
// to base, which is how ESP-IDF's encoder chaining works.
typedef struct {
    rmt_encoder_t base;         // must be first
    rmt_encoder_t *bytes_encoder;  // encodes pixel bytes -> RMT symbols
    rmt_encoder_t *copy_encoder;   // encodes the reset pulse
    rmt_symbol_word_t reset_code;  // the pre-built reset symbol
    int state;                     // tracks encoder state machine position
} rmt_led_strip_encoder_t;

// ----------------------------------------------------------------
// Pixel buffer — stores RGB values set by led_strip_set_pixel()
// ----------------------------------------------------------------
static uint8_t s_pixel_buf[LED_STRIP_LED_COUNT * 3]; // 3 bytes per LED (R,G,B)

// ----------------------------------------------------------------
// RMT handles — allocated during led_strip_init()
// ----------------------------------------------------------------
static rmt_channel_handle_t s_rmt_channel = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;

// ----------------------------------------------------------------
// Custom encoder encode function
// Called by the RMT driver each time it needs more symbols.
// Runs in ISR context — must not call blocking APIs.
// ----------------------------------------------------------------
static size_t rmt_encode_led_strip(rmt_encoder_t *encoder,
                                    rmt_channel_handle_t channel,
                                    const void *primary_data,
                                    size_t data_size,
                                    rmt_encode_state_t *ret_state)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);

    rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
    rmt_encoder_handle_t copy_encoder  = led_encoder->copy_encoder;

    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state         = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (led_encoder->state) {
        case 0: // Phase 1: encode all pixel bytes
            encoded_symbols += bytes_encoder->encode(
                bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = 1;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out;
            }
            __attribute__((fallthrough));
        case 1: // Phase 2: send reset pulse
            encoded_symbols += copy_encoder->encode(
                copy_encoder, channel,
                &led_encoder->reset_code, sizeof(led_encoder->reset_code),
                &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = RMT_ENCODING_RESET;
                state |= RMT_ENCODING_COMPLETE;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out;
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

// Called by the RMT driver to reset encoder state between transmissions
static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

// Called when the encoder is deleted — frees all sub-encoders and memory
static esp_err_t rmt_led_strip_encoder_del(rmt_encoder_t *encoder)
{
    rmt_led_strip_encoder_t *led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------

void led_strip_init(void)
{
    ESP_LOGI(TAG, "Initialising LED strip on GPIO %d", LED_STRIP_GPIO_PIN);

    // --- Configure and create the RMT TX channel ---
    // The channel is the hardware path from the ESP32 GPIO to the LED data pin.
    rmt_tx_channel_config_t channel_cfg = {
        .gpio_num        = LED_STRIP_GPIO_PIN,
        .clk_src         = RMT_CLK_SRC_DEFAULT, // use the default clock source
        .resolution_hz   = LED_STRIP_RMT_RES_HZ, // 10MHz = 100ns per tick
        .mem_block_symbols = 64, // number of RMT symbols the hardware buffer holds
        .trans_queue_depth = 4,  // number of pending transmissions allowed
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&channel_cfg, &s_rmt_channel));

    // --- Build the custom led strip encoder ---
    // Allocate memory for our encoder struct in RMT-safe memory
    rmt_led_strip_encoder_t *led_encoder =
        rmt_alloc_encoder_mem(sizeof(rmt_led_strip_encoder_t));
    if (!led_encoder) {
        ESP_LOGE(TAG, "Failed to allocate encoder memory");
        abort();
    }

    // Wire up the function pointers the RMT driver calls
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del    = rmt_led_strip_encoder_del;
    led_encoder->base.reset  = rmt_led_strip_encoder_reset;
    led_encoder->state       = 0;

    // --- Configure the bytes encoder ---
    // bit0: 300ns high (3 ticks at 10MHz), 900ns low (9 ticks)
    // bit1: 900ns high (9 ticks), 300ns low (3 ticks)
    // msb_first = true because WS2812B expects MSB first, GRB order
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .level0    = 1, .duration0 = 3,  // 300ns high
            .level1    = 0, .duration1 = 9,  // 900ns low
        },
        .bit1 = {
            .level0    = 1, .duration0 = 9,  // 900ns high
            .level1    = 0, .duration1 = 3,  // 300ns low
        },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_cfg, &led_encoder->bytes_encoder));

    // --- Configure the copy encoder for the reset pulse ---
    // The reset pulse is 50µs low — split into two 25µs (250 tick) halves
    // because rmt_symbol_word_t has two duration fields
    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &led_encoder->copy_encoder));

    uint32_t reset_ticks = LED_STRIP_RMT_RES_HZ / 1000000 * 50 / 2; // = 250 ticks = 25µs
    led_encoder->reset_code = (rmt_symbol_word_t){
        .level0    = 0, .duration0 = reset_ticks,
        .level1    = 0, .duration1 = reset_ticks,
    };

    s_led_encoder = &led_encoder->base;

    // --- Enable the RMT channel ---
    // The channel must be enabled before any transmission
    ESP_ERROR_CHECK(rmt_enable(s_rmt_channel));

    ESP_LOGI(TAG, "LED strip initialised, %d LED(s)", LED_STRIP_LED_COUNT);
}

void led_strip_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= LED_STRIP_LED_COUNT) {
        ESP_LOGW(TAG, "led_strip_set_pixel: index %lu out of range", (unsigned long)index);
        return;
    }
    // Store in RGB order — conversion to GRB happens in led_strip_refresh()
    s_pixel_buf[index * 3 + 0] = r;
    s_pixel_buf[index * 3 + 1] = g;
    s_pixel_buf[index * 3 + 2] = b;
}

void led_strip_refresh(void)
{
    // WS2812B expects GRB order, but we store RGB in the buffer.
    // Build a temporary GRB transmit buffer for the RMT driver.
    uint8_t grb_buf[LED_STRIP_LED_COUNT * 3];
    for (uint32_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        grb_buf[i * 3 + 0] = s_pixel_buf[i * 3 + 1]; // G
        grb_buf[i * 3 + 1] = s_pixel_buf[i * 3 + 0]; // R
        grb_buf[i * 3 + 2] = s_pixel_buf[i * 3 + 2]; // B
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0, // transmit once, no looping
    };
    ESP_ERROR_CHECK(rmt_transmit(s_rmt_channel, s_led_encoder,
                                  grb_buf, sizeof(grb_buf), &tx_cfg));

    // Block until the transmission is complete before returning.
    // This ensures the LED has latched before the next call.
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_rmt_channel, -1));
}

void led_strip_clear(void)
{
    memset(s_pixel_buf, 0, sizeof(s_pixel_buf));
    led_strip_refresh();
}