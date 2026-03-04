#include "rec_power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sensecap-watcher.h"

#define REC_POWER_RECORDING_BRIGHTNESS  70
#define REC_POWER_STANDBY_BRIGHTNESS    0
#define REC_POWER_LCD_STABLE_MS         20
#define REC_POWER_BAT_ADC_SETTLE_MS     8

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
    (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0);
    s_initialized = true;
    return ESP_OK;
}

void rec_power_set_recording_mode(bool recording_on)
{
    s_recording_mode = recording_on;
    int target = recording_on ? REC_POWER_RECORDING_BRIGHTNESS : REC_POWER_STANDBY_BRIGHTNESS;
    if (recording_on) {
        (void)bsp_exp_io_set_level(BSP_PWR_LCD, 1);
        (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1);
        vTaskDelay(pdMS_TO_TICKS(REC_POWER_LCD_STABLE_MS));
    }
    (void)bsp_lcd_brightness_set(target);
    if (!recording_on) {
        (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0);
        (void)bsp_exp_io_set_level(BSP_PWR_LCD, 0);
    }
}

bool rec_power_is_recording_mode(void)
{
    return s_recording_mode;
}

uint8_t rec_power_read_battery_percent(void)
{
    if (!s_initialized) {
        return 0;
    }

    bool temp_power_on = !s_recording_mode;
    if (temp_power_on) {
        (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1);
        vTaskDelay(pdMS_TO_TICKS(REC_POWER_BAT_ADC_SETTLE_MS));
    }

    uint8_t percent = bsp_battery_get_percent();

    if (temp_power_on) {
        (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0);
    }
    return percent;
}
