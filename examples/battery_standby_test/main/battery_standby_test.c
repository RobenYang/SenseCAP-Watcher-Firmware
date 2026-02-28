/*
 * SPDX-FileCopyrightText: 2026 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "sensecap-watcher.h"

#define SCREEN_ON_BRIGHTNESS 50
#define BUTTON_LONG_PRESS_MS 1500
#define BUTTON_SHORT_PRESS_MS 180
#define BATTERY_UPDATE_MS 1000

static const char *TAG = "battery_test";

static lv_obj_t *s_battery_label = NULL;
static lv_timer_t *s_battery_timer = NULL;
static lv_indev_t *s_touch_indev = NULL;
static button_handle_t s_knob_button = NULL;
static bool s_screen_on = true;
static bool s_long_press_active = false;
static uint8_t s_last_battery_percent = 0xFF;

static void battery_update_timer_cb(lv_timer_t *timer);
static void set_screen_enabled(bool enabled);

/*
 * Power rails we keep disabled all the time for this battery standby test firmware.
 * Only display + knob wake path are preserved.
 */
static const uint16_t s_power_always_off_mask = BSP_PWR_AI_CHIP | BSP_PWR_GROVE | BSP_PWR_SDCARD | BSP_PWR_CODEC_PA;

static void apply_static_power_policy(void)
{
    ESP_ERROR_CHECK(bsp_exp_io_set_level(s_power_always_off_mask, 0));
}

static void enter_deep_sleep_now(void)
{
    ESP_LOGI(TAG, "Entering deep sleep");

    if (s_screen_on)
    {
        set_screen_enabled(false);
    }

    esp_lcd_panel_handle_t panel = bsp_lcd_get_panel_handle();
    if (panel)
    {
        esp_lcd_panel_disp_on_off(panel, false);
    }

    apply_static_power_policy();
    vTaskDelay(pdMS_TO_TICKS(30));
    bsp_system_deep_sleep(0);
}

static lv_indev_t *find_touch_indev(void)
{
    lv_indev_t *indev = NULL;
    while (1)
    {
        indev = lv_indev_get_next(indev);
        if (indev == NULL)
        {
            return NULL;
        }
        if (indev->driver->type == LV_INDEV_TYPE_POINTER)
        {
            return indev;
        }
    }
}

static void set_screen_enabled(bool enabled)
{
    if (enabled == s_screen_on)
    {
        return;
    }

    if (enabled)
    {
        /* Re-enable battery ADC and touch before turning on the backlight. */
        ESP_ERROR_CHECK(bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1));

        lvgl_port_lock(portMAX_DELAY);
        if (s_touch_indev)
        {
            lv_indev_enable(s_touch_indev, true);
        }
        if (s_battery_timer)
        {
            lv_timer_resume(s_battery_timer);
        }
        lvgl_port_unlock();
    }
    else
    {
        /* Screen-off mode: only keep knob wake path alive. */
        lvgl_port_lock(portMAX_DELAY);
        if (s_touch_indev)
        {
            lv_indev_enable(s_touch_indev, false);
        }
        if (s_battery_timer)
        {
            lv_timer_pause(s_battery_timer);
        }
        lvgl_port_unlock();

        ESP_ERROR_CHECK(bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0));
    }

    s_screen_on = enabled;
    ESP_ERROR_CHECK(bsp_lcd_brightness_set(enabled ? SCREEN_ON_BRIGHTNESS : 0));

    if (enabled)
    {
        lvgl_port_lock(portMAX_DELAY);
        battery_update_timer_cb(NULL);
        lvgl_port_unlock();
    }
}

static void battery_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_screen_on)
    {
        return;
    }

    uint8_t battery_percent = bsp_battery_get_percent();
    if (battery_percent == s_last_battery_percent)
    {
        return;
    }

    s_last_battery_percent = battery_percent;
    char battery_text[16] = {0};

    lv_snprintf(battery_text, sizeof(battery_text), "%u%%", battery_percent);
    lv_label_set_text(s_battery_label, battery_text);
    lv_obj_center(s_battery_label);
}

static void knob_press_down_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;
    s_long_press_active = false;
}

static void knob_press_up_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;

    if (s_long_press_active)
    {
        s_long_press_active = false;
        return;
    }

    enter_deep_sleep_now();
}

static void knob_long_press_cb(void *button_handle, void *usr_data)
{
    (void)button_handle;
    (void)usr_data;

    s_long_press_active = true;
    ESP_LOGI(TAG, "Long press detected, shutting down");
    if (s_screen_on)
    {
        set_screen_enabled(false);
    }

    apply_static_power_policy();
    vTaskDelay(pdMS_TO_TICKS(100));
    bsp_system_shutdown();

    /*
     * When powered via USB-C, hardware might not fully cut power.
     * Restarting keeps behavior deterministic for repeated tests.
     */
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}

static void knob_button_init(void)
{
    const button_config_t knob_cfg = {
        .type = BUTTON_TYPE_CUSTOM,
        .long_press_time = BUTTON_LONG_PRESS_MS,
        .short_press_time = BUTTON_SHORT_PRESS_MS,
        .custom_button_config = {
            .active_level = 0,
            .button_custom_init = bsp_knob_btn_init,
            .button_custom_deinit = bsp_knob_btn_deinit,
            .button_custom_get_key_value = bsp_knob_btn_get_key_value,
        },
    };

    s_knob_button = iot_button_create(&knob_cfg);
    assert(s_knob_button != NULL);

    ESP_ERROR_CHECK(iot_button_register_cb(s_knob_button, BUTTON_PRESS_DOWN, knob_press_down_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(s_knob_button, BUTTON_PRESS_UP, knob_press_up_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(s_knob_button, BUTTON_LONG_PRESS_START, knob_long_press_cb, NULL));
}

static void battery_ui_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_battery_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_battery_label, lv_color_white(), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_32, LV_PART_MAIN);
#endif
    lv_label_set_text(s_battery_label, "--%");
    lv_obj_center(s_battery_label);

    battery_update_timer_cb(NULL);
    s_battery_timer = lv_timer_create(battery_update_timer_cb, BATTERY_UPDATE_MS, NULL);
    assert(s_battery_timer != NULL);
}

void app_main(void)
{
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Wakeup cause: %d", wakeup_cause);

    bsp_io_expander_init();
    apply_static_power_policy();

    lv_disp_t *lvgl_disp = bsp_lvgl_init();
    assert(lvgl_disp != NULL);

    /* Reduce log overhead in idle/standby test. */
    esp_log_level_set("BSP", ESP_LOG_ERROR);

    ESP_ERROR_CHECK(bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1));

    lvgl_port_lock(portMAX_DELAY);
    s_touch_indev = find_touch_indev();
    battery_ui_init();
    lvgl_port_unlock();

    knob_button_init();
    s_screen_on = false;
    set_screen_enabled(true);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
