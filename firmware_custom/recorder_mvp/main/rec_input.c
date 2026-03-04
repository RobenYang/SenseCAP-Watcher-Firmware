#include "rec_input.h"

#include "esp_timer.h"

#include "sensecap-watcher.h"

#define REC_INPUT_DEBOUNCE_MS      25
#define REC_INPUT_MIN_PRESS_MS     30
#define REC_INPUT_MAX_SHORT_MS     1200
#define REC_INPUT_LONG_PRESS_MS    1800

static bool s_initialized;
static bool s_raw_pressed;
static bool s_stable_pressed;
static bool s_short_press_pending;
static bool s_long_press_pending;
static bool s_long_press_fired;
static uint32_t s_raw_change_ms;
static uint32_t s_press_start_ms;

static inline uint32_t rec_input_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

esp_err_t rec_input_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (bsp_io_expander_init() == NULL) {
        return ESP_FAIL;
    }

    uint32_t now = rec_input_ms();
    s_raw_pressed = (bsp_exp_io_get_level(BSP_KNOB_BTN) == 0);
    s_stable_pressed = s_raw_pressed;
    s_raw_change_ms = now;
    s_press_start_ms = now;
    s_short_press_pending = false;
    s_long_press_pending = false;
    s_long_press_fired = false;
    s_initialized = true;
    return ESP_OK;
}

void rec_input_update(void)
{
    if (!s_initialized) {
        return;
    }

    uint32_t now = rec_input_ms();
    bool raw_pressed = (bsp_exp_io_get_level(BSP_KNOB_BTN) == 0);
    if (raw_pressed != s_raw_pressed) {
        s_raw_pressed = raw_pressed;
        s_raw_change_ms = now;
    }

    if ((now - s_raw_change_ms) < REC_INPUT_DEBOUNCE_MS) {
        return;
    }

    if (s_stable_pressed == s_raw_pressed) {
        if (s_stable_pressed && !s_long_press_fired) {
            uint32_t hold_ms = now - s_press_start_ms;
            if (hold_ms >= REC_INPUT_LONG_PRESS_MS) {
                s_long_press_pending = true;
                s_long_press_fired = true;
            }
        }
        return;
    }

    s_stable_pressed = s_raw_pressed;
    if (s_stable_pressed) {
        s_press_start_ms = now;
        s_long_press_fired = false;
        return;
    }

    uint32_t press_ms = now - s_press_start_ms;
    if (!s_long_press_fired &&
        press_ms >= REC_INPUT_MIN_PRESS_MS &&
        press_ms <= REC_INPUT_MAX_SHORT_MS) {
        s_short_press_pending = true;
    }
}

bool rec_input_take_short_press(void)
{
    bool pending = s_short_press_pending;
    s_short_press_pending = false;
    return pending;
}

bool rec_input_take_long_press(void)
{
    bool pending = s_long_press_pending;
    s_long_press_pending = false;
    return pending;
}
