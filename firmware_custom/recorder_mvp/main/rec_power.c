#include "rec_power.h"

#include "sensecap-watcher.h"

#define REC_POWER_RECORDING_BRIGHTNESS  70
#define REC_POWER_STANDBY_BRIGHTNESS    8

static bool s_recording_mode;
static bool s_initialized;

esp_err_t rec_power_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (bsp_io_expander_init() == NULL) {
        return ESP_FAIL;
    }

    s_recording_mode = false;
    s_initialized = true;
    return ESP_OK;
}

void rec_power_set_recording_mode(bool recording_on)
{
    s_recording_mode = recording_on;
    int target = recording_on ? REC_POWER_RECORDING_BRIGHTNESS : REC_POWER_STANDBY_BRIGHTNESS;
    (void)bsp_lcd_brightness_set(target);
}

bool rec_power_is_recording_mode(void)
{
    return s_recording_mode;
}
