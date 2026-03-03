#include "rec_led.h"

#include "sensecap-watcher.h"

#define REC_LED_RMS_NOISE_GATE   500
#define REC_LED_RMS_FULL_SCALE   8000
#define REC_LED_BRIGHTNESS_CAP   200

static bool s_initialized;
static bool s_enabled;
static int s_current_brightness;

static inline uint8_t rec_led_map_rms(uint16_t rms)
{
    if (rms <= REC_LED_RMS_NOISE_GATE) {
        return 0;
    }

    int32_t value = (int32_t)rms - REC_LED_RMS_NOISE_GATE;
    int32_t span = REC_LED_RMS_FULL_SCALE - REC_LED_RMS_NOISE_GATE;
    if (span <= 0) {
        return 0;
    }

    value = (value * REC_LED_BRIGHTNESS_CAP) / span;
    if (value < 0) {
        value = 0;
    } else if (value > REC_LED_BRIGHTNESS_CAP) {
        value = REC_LED_BRIGHTNESS_CAP;
    }
    return (uint8_t)value;
}

esp_err_t rec_led_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = bsp_rgb_init();
    if (ret != ESP_OK) {
        return ret;
    }

    s_enabled = false;
    s_current_brightness = 0;
    s_initialized = true;
    return ESP_OK;
}

void rec_led_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (!s_initialized) {
        return;
    }

    if (!enabled) {
        s_current_brightness = 0;
        (void)bsp_rgb_set(0, 0, 0);
    }
}

void rec_led_update_rms(uint16_t rms)
{
    if (!s_initialized || !s_enabled) {
        return;
    }

    int target = rec_led_map_rms(rms);
    int delta = target - s_current_brightness;
    if (delta > 0) {
        s_current_brightness += (delta * 5) / 10;
    } else {
        s_current_brightness += (delta * 2) / 10;
    }
    if (s_current_brightness < 0) {
        s_current_brightness = 0;
    } else if (s_current_brightness > REC_LED_BRIGHTNESS_CAP) {
        s_current_brightness = REC_LED_BRIGHTNESS_CAP;
    }

    uint8_t white = (uint8_t)s_current_brightness;
    (void)bsp_rgb_set(white, white, white);
}
