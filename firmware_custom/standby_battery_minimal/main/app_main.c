#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_private/esp_clk.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "sensecap-watcher.h"

#define APP_BRIGHTNESS_PERCENT          10
#define BUTTON_POLL_MS                  40
#define BUTTON_DEBOUNCE_MS              30
#define BATTERY_SETTLE_MS               250
#define BATTERY_LCD_OFF_SETTLE_MS       400
#define BATTERY_SAMPLE_INTERVAL_MS      25
#define BATTERY_DISCARD_SAMPLES         2
#define BATTERY_SAMPLE_COUNT            6
#define BATTERY_NOISE_FLOOR_MV          3
#define BATTERY_NOISE_FLOOR_PERCENT     0.02f
#define UI_REFRESH_MS                   5000
#define STANDBY_STATE_MAGIC             0x53544259UL

static const char *TAG = "standby_minimal";

typedef struct
{
    uint16_t voltage_mv;
    float percent_est;
    uint8_t percent_rounded;
} battery_sample_t;

typedef struct
{
    bool has_previous_sample;
    uint32_t duration_sec;
    int32_t delta_mv;
    float delta_percent;
    float rate_percent_per_hour;
} standby_report_t;

typedef struct
{
    uint32_t magic;
    uint64_t sleep_entry_rtc_us;
    uint16_t sleep_entry_voltage_mv;
    float sleep_entry_percent;
} standby_state_rtc_t;

RTC_DATA_ATTR static standby_state_rtc_t s_standby_state = { 0 };

static const uint16_t s_nonessential_rail_mask = BSP_PWR_SDCARD | BSP_PWR_CODEC_PA | BSP_PWR_AI_CHIP | BSP_PWR_GROVE;
static lv_obj_t *s_battery_label = NULL;
static lv_obj_t *s_voltage_label = NULL;
static lv_obj_t *s_standby_label = NULL;
static lv_obj_t *s_duration_label = NULL;
static lv_obj_t *s_hint_label = NULL;

static float clampf_range(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static float estimate_battery_percent_from_mv(uint16_t voltage_mv)
{
    float voltage = (float)voltage_mv;
    float percent = (-1.0f * voltage * voltage + 9016.0f * voltage - 19189000.0f) / 10000.0f;
    return clampf_range(percent, 0.0f, 100.0f);
}

static bool is_button_pressed(void)
{
    return bsp_exp_io_get_level(BSP_KNOB_BTN) == 0;
}

static void wait_for_button_release(void)
{
    while (is_button_pressed())
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(80));
}

static void force_unused_power_domains_off(bool keep_lcd_on)
{
    uint16_t mask = s_nonessential_rail_mask | BSP_PWR_BAT_ADC;
    if (!keep_lcd_on)
    {
        mask |= BSP_PWR_LCD;
    }
    ESP_ERROR_CHECK(bsp_exp_io_set_level(mask, 0));
}

static battery_sample_t sample_battery_low_load(void)
{
    battery_sample_t sample = { 0 };
    uint32_t total_mv = 0;

    ESP_ERROR_CHECK(bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1));
    vTaskDelay(pdMS_TO_TICKS(BATTERY_SETTLE_MS));

    for (uint8_t index = 0; index < BATTERY_DISCARD_SAMPLES; ++index)
    {
        (void)bsp_battery_get_voltage();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_SAMPLE_INTERVAL_MS));
    }

    for (uint8_t index = 0; index < BATTERY_SAMPLE_COUNT; ++index)
    {
        total_mv += bsp_battery_get_voltage();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_SAMPLE_INTERVAL_MS));
    }

    ESP_ERROR_CHECK(bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0));

    sample.voltage_mv = (uint16_t)(total_mv / BATTERY_SAMPLE_COUNT);
    sample.percent_est = estimate_battery_percent_from_mv(sample.voltage_mv);
    sample.percent_rounded = (uint8_t)lroundf(sample.percent_est);
    return sample;
}

static standby_report_t calculate_standby_report(const battery_sample_t *wake_sample)
{
    standby_report_t report = { 0 };

    if ((esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) || (s_standby_state.magic != STANDBY_STATE_MAGIC))
    {
        return report;
    }

    uint64_t now_rtc_us = esp_clk_rtc_time();
    if (now_rtc_us <= s_standby_state.sleep_entry_rtc_us)
    {
        return report;
    }

    report.has_previous_sample = true;
    report.duration_sec = (uint32_t)((now_rtc_us - s_standby_state.sleep_entry_rtc_us) / 1000000ULL);
    report.delta_mv = (int32_t)s_standby_state.sleep_entry_voltage_mv - (int32_t)wake_sample->voltage_mv;
    report.delta_percent = s_standby_state.sleep_entry_percent - wake_sample->percent_est;

    if ((abs(report.delta_mv) <= BATTERY_NOISE_FLOOR_MV) && (fabsf(report.delta_percent) < BATTERY_NOISE_FLOOR_PERCENT))
    {
        report.delta_mv = 0;
        report.delta_percent = 0.0f;
    }

    if (report.delta_mv < 0)
    {
        report.delta_mv = 0;
    }
    if (report.delta_percent < 0.0f)
    {
        report.delta_percent = 0.0f;
    }

    if (report.duration_sec > 0)
    {
        report.rate_percent_per_hour = report.delta_percent * (3600.0f / (float)report.duration_sec);
    }

    return report;
}

static void format_rate_text(const standby_report_t *report, char *buffer, size_t buffer_size)
{
    if (!report->has_previous_sample)
    {
        snprintf(buffer, buffer_size, "Standby N/A");
        return;
    }

    snprintf(buffer, buffer_size, "Standby %.2f%%/h", (double)report->rate_percent_per_hour);
}

static void format_duration_text(const standby_report_t *report, char *buffer, size_t buffer_size)
{
    if (!report->has_previous_sample)
    {
        snprintf(buffer, buffer_size, "Sleep N/A");
        return;
    }

    uint32_t hours = report->duration_sec / 3600U;
    uint32_t minutes = (report->duration_sec % 3600U) / 60U;
    uint32_t seconds = report->duration_sec % 60U;

    if (hours > 0)
    {
        snprintf(buffer, buffer_size, "Sleep %luh %02lum", (unsigned long)hours, (unsigned long)minutes);
        return;
    }

    if (minutes > 0)
    {
        snprintf(buffer, buffer_size, "Sleep %lum %02lus", (unsigned long)minutes, (unsigned long)seconds);
        return;
    }

    snprintf(buffer, buffer_size, "Sleep %lus", (unsigned long)seconds);
}

static void create_ui_locked(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_battery_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_battery_label, lv_color_white(), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_48, LV_PART_MAIN);
#endif
    lv_obj_align(s_battery_label, LV_ALIGN_CENTER, 0, -64);

    s_voltage_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_voltage_label, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_voltage_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(s_voltage_label, LV_ALIGN_CENTER, 0, -6);

    s_standby_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_standby_label, lv_color_hex(0xA0A0A0), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_standby_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(s_standby_label, LV_ALIGN_CENTER, 0, 42);

    s_duration_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_duration_label, lv_color_hex(0xA0A0A0), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_duration_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(s_duration_label, LV_ALIGN_CENTER, 0, 78);

    s_hint_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_hint_label, lv_color_hex(0x6F6F6F), LV_PART_MAIN);
    lv_label_set_text(s_hint_label, "Press knob to sleep");
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void update_ui_locked(const battery_sample_t *battery_sample, const standby_report_t *report)
{
    char percent_text[16] = { 0 };
    char voltage_text[24] = { 0 };
    char rate_text[32] = { 0 };
    char duration_text[24] = { 0 };

    if (battery_sample->voltage_mv == 0)
    {
        snprintf(percent_text, sizeof(percent_text), "N/A");
        snprintf(voltage_text, sizeof(voltage_text), "Battery ADC N/A");
    }
    else
    {
        snprintf(percent_text, sizeof(percent_text), "%u%%", battery_sample->percent_rounded);
        snprintf(voltage_text, sizeof(voltage_text), "Vbat %.3fV", (double)battery_sample->voltage_mv / 1000.0);
    }

    format_rate_text(report, rate_text, sizeof(rate_text));
    format_duration_text(report, duration_text, sizeof(duration_text));

    lv_label_set_text(s_battery_label, percent_text);
    lv_label_set_text(s_voltage_label, voltage_text);
    lv_label_set_text(s_standby_label, rate_text);
    lv_label_set_text(s_duration_label, duration_text);
}

static void render_ui(const battery_sample_t *battery_sample, const standby_report_t *report)
{
    if (!lvgl_port_lock(1000))
    {
        ESP_LOGW(TAG, "Failed to lock LVGL");
        return;
    }

    if (s_battery_label == NULL)
    {
        create_ui_locked();
    }

    update_ui_locked(battery_sample, report);
    lv_refr_now(NULL);
    lvgl_port_unlock();
}

static void blank_display_before_sleep(void)
{
    if (!lvgl_port_lock(1000))
    {
        return;
    }

    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_refr_now(NULL);
    lvgl_port_unlock();
}

static void record_sleep_entry_sample(void)
{
    battery_sample_t entry_sample = sample_battery_low_load();
    s_standby_state.magic = STANDBY_STATE_MAGIC;
    s_standby_state.sleep_entry_rtc_us = esp_clk_rtc_time();
    s_standby_state.sleep_entry_voltage_mv = entry_sample.voltage_mv;
    s_standby_state.sleep_entry_percent = entry_sample.percent_est;
    ESP_LOGW(TAG, "Sleep entry: %u mV, %.2f%%", entry_sample.voltage_mv, (double)entry_sample.percent_est);
}

static void enter_standby_now(void)
{
    esp_lcd_panel_handle_t panel = bsp_lcd_get_panel_handle();
    blank_display_before_sleep();
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_lcd_brightness_set(0));
    if (panel != NULL)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel, false));
    }

    force_unused_power_domains_off(false);
    vTaskDelay(pdMS_TO_TICKS(BATTERY_LCD_OFF_SETTLE_MS));
    record_sleep_entry_sample();
    wait_for_button_release();
    force_unused_power_domains_off(false);
    bsp_system_deep_sleep(0);
}

void app_main(void)
{
    ESP_LOGW(TAG, "Minimal standby battery firmware start");

    if (bsp_io_expander_init() == NULL)
    {
        ESP_LOGE(TAG, "IO expander init failed");
        abort();
    }

    force_unused_power_domains_off(false);

    battery_sample_t wake_sample = sample_battery_low_load();
    standby_report_t standby_report = calculate_standby_report(&wake_sample);
    ESP_LOGW(TAG, "Wake sample: %u mV, %.2f%%, wakeup=%d", wake_sample.voltage_mv, (double)wake_sample.percent_est, (int)esp_sleep_get_wakeup_cause());
    if (standby_report.has_previous_sample)
    {
        ESP_LOGW(TAG,
            "Standby sample: %lus, delta=%d mV, %.4f%%, rate=%.4f%%/h",
            (unsigned long)standby_report.duration_sec,
            (int)standby_report.delta_mv,
            (double)standby_report.delta_percent,
            (double)standby_report.rate_percent_per_hour);
    }

    if (bsp_lvgl_init() == NULL)
    {
        ESP_LOGE(TAG, "LVGL display init failed");
        abort();
    }

    render_ui(&wake_sample, &standby_report);
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_lcd_brightness_set(APP_BRIGHTNESS_PERCENT));
    wait_for_button_release();

    bool previous_pressed = false;
    int64_t last_ui_refresh_us = esp_timer_get_time();
    while (true)
    {
        bool pressed = is_button_pressed();
        if (pressed && !previous_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (is_button_pressed())
            {
                enter_standby_now();
            }
        }

        int64_t now_us = esp_timer_get_time();
        if ((now_us - last_ui_refresh_us) >= (UI_REFRESH_MS * 1000LL))
        {
            wake_sample = sample_battery_low_load();
            ESP_LOGW(TAG, "Live sample: %u mV, %.2f%%", wake_sample.voltage_mv, (double)wake_sample.percent_est);
            render_ui(&wake_sample, &standby_report);
            last_ui_refresh_us = now_us;
        }

        previous_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}
