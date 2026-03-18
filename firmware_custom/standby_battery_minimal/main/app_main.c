#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_private/esp_clk.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "sensecap-watcher.h"

#define APP_BRIGHTNESS_PERCENT         10
#define BUTTON_POLL_MS                 20
#define BUTTON_DEBOUNCE_MS             30
#define BATTERY_QUICK_SETTLE_MS        60
#define BATTERY_STABLE_SETTLE_MS       250
#define BATTERY_LCD_OFF_SETTLE_MS      400
#define BATTERY_SAMPLE_INTERVAL_MS     25
#define BATTERY_QUICK_DISCARD_SAMPLES  1
#define BATTERY_QUICK_SAMPLE_COUNT     4
#define BATTERY_STABLE_DISCARD_SAMPLES 2
#define BATTERY_STABLE_SAMPLE_COUNT    6
#define UI_REFRESH_MS                  1000
#define USB_STATUS_REFRESH_MS          1000
#define USB_BATTERY_REFRESH_MS         15000
#define CHARGE_ETA_MINUTES_PER_PERCENT 2
#define LED_FLASH_ON_MS                60
#define LED_FLASH_OFF_MS               1940
#define LED_ERROR_BLINK_MS             160
#define LED_WHITE_LEVEL                64
#define LED_RED_LEVEL                  128
#define RECORD_BUFFER_BYTES            (128 * 1024)
#define RECORD_CHUNK_BYTES             4096
#define RECORD_CHUNK_COUNT             (RECORD_BUFFER_BYTES / RECORD_CHUNK_BYTES)
#define RECORD_QUEUE_WAIT_MS           50
#define RECORD_TASK_STACK_SIZE         4096
#define WRITER_TASK_STACK_SIZE         4096
#define LED_TASK_STACK_SIZE            2048
#define RECORD_TASK_PRIORITY           8
#define WRITER_TASK_PRIORITY           6
#define LED_TASK_PRIORITY              2
#define LOG_PATH                       "/sdcard/events.csv"
#define RECORD_PATH_FMT                "/sdcard/r%05lu.pcm"
#define RECORD_NAME_FMT                "r%05lu.pcm"
#define RTC_STATE_MAGIC                0x52454352UL

#if (RECORD_BUFFER_BYTES % RECORD_CHUNK_BYTES) != 0
#error "RECORD_BUFFER_BYTES must be divisible by RECORD_CHUNK_BYTES"
#endif

static const char *TAG = "recorder_minimal";

typedef struct
{
    bool valid;
    uint16_t voltage_mv;
    float percent_est;
    uint8_t percent_rounded;
} battery_sample_t;

typedef enum
{
    UI_STATE_WAKING = 0,
    UI_STATE_RECORDING,
    UI_STATE_SAVING,
    UI_STATE_ERROR,
} ui_state_t;

typedef enum
{
    APP_ERR_NONE = 0,
    APP_ERR_RGB_INIT,
    APP_ERR_RECORD_BUFFER,
    APP_ERR_AUDIO_INIT,
    APP_ERR_RECORD_TASK,
    APP_ERR_WRITE_TASK,
    APP_ERR_NO_SD,
    APP_ERR_SD_MOUNT,
    APP_ERR_RECORD_OPEN,
    APP_ERR_LOG_OPEN,
    APP_ERR_DISPLAY_INIT,
    APP_ERR_AUDIO_READ,
    APP_ERR_SD_WRITE,
    APP_ERR_LOG_WRITE,
} app_error_t;

typedef struct
{
    uint32_t magic;
    uint32_t next_session_seq;
    uint8_t last_vbus_present;
} recorder_rtc_state_t;

typedef struct
{
    uint16_t chunk_index;
    size_t bytes;
    bool sentinel;
} record_chunk_msg_t;

typedef struct
{
    uint8_t *storage;
    QueueHandle_t free_queue;
    QueueHandle_t filled_queue;
    TaskHandle_t capture_task;
    TaskHandle_t writer_task;
    volatile bool stop_requested;
    volatile bool capture_done;
    volatile bool writer_done;
    volatile size_t bytes_written;
} record_pipe_t;

RTC_DATA_ATTR static recorder_rtc_state_t s_rtc_state = { 0 };

static record_pipe_t s_record_pipe = { 0 };
static FILE *s_record_file = NULL;
static FILE *s_log_file = NULL;
static char s_record_path[64] = { 0 };
static char s_record_name[32] = { 0 };
static uint32_t s_session_seq = 0;
static int s_wakeup_cause = 0;
static int64_t s_record_start_us = 0;
static bool s_audio_started = false;
static bool s_display_ready = false;
static bool s_rgb_ready = false;
static bool s_sd_powered = false;
static bool s_sd_mounted = false;
static volatile app_error_t s_runtime_error = APP_ERR_NONE;
static TaskHandle_t s_led_task = NULL;
static volatile bool s_led_stop_requested = false;
static volatile bool s_led_task_running = false;
static battery_sample_t s_display_battery = { 0 };
static lv_obj_t *s_battery_label = NULL;
static lv_obj_t *s_duration_label = NULL;
static lv_obj_t *s_status_label = NULL;

static void disable_display_for_sleep(void);
static void enter_deep_sleep_now(void);

static bool is_vbus_present(void)
{
    return bsp_exp_io_get_level(BSP_PWR_VBUS_IN_DET) != 0;
}

static bool is_usb_data_connected(void)
{
    return usb_serial_jtag_is_connected();
}

static bool was_vbus_present_before_boot(void)
{
    return s_rtc_state.last_vbus_present != 0;
}

static void set_rtc_vbus_present(bool present)
{
    s_rtc_state.last_vbus_present = present ? 1U : 0U;
}

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

static esp_err_t force_optional_power_domains_off(bool keep_sd_on, bool keep_lcd_on)
{
    uint16_t mask = BSP_PWR_CODEC_PA | BSP_PWR_AI_CHIP | BSP_PWR_GROVE | BSP_PWR_BAT_ADC;
    if (!keep_sd_on)
    {
        mask |= BSP_PWR_SDCARD;
    }
    if (!keep_lcd_on)
    {
        mask |= BSP_PWR_LCD;
    }
    return bsp_exp_io_set_level(mask, 0);
}

static esp_err_t set_sd_power(bool enabled)
{
    esp_err_t ret = bsp_exp_io_set_level(BSP_PWR_SDCARD, enabled ? 1 : 0);
    if (ret == ESP_OK)
    {
        s_sd_powered = enabled;
    }
    return ret;
}

static battery_sample_t sample_battery_window(uint32_t settle_ms, uint8_t discard_count, uint8_t sample_count)
{
    battery_sample_t sample = { 0 };
    uint32_t total_mv = 0;

    if (bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 1) != ESP_OK)
    {
        return sample;
    }

    vTaskDelay(pdMS_TO_TICKS(settle_ms));

    for (uint8_t index = 0; index < discard_count; ++index)
    {
        (void)bsp_battery_get_voltage();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_SAMPLE_INTERVAL_MS));
    }

    for (uint8_t index = 0; index < sample_count; ++index)
    {
        total_mv += bsp_battery_get_voltage();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_SAMPLE_INTERVAL_MS));
    }

    (void)bsp_exp_io_set_level(BSP_PWR_BAT_ADC, 0);

    if (sample_count == 0)
    {
        return sample;
    }

    sample.valid = true;
    sample.voltage_mv = (uint16_t)(total_mv / sample_count);
    sample.percent_est = estimate_battery_percent_from_mv(sample.voltage_mv);
    sample.percent_rounded = (uint8_t)lroundf(sample.percent_est);
    return sample;
}

static battery_sample_t sample_battery_quick(void)
{
    return sample_battery_window(BATTERY_QUICK_SETTLE_MS, BATTERY_QUICK_DISCARD_SAMPLES, BATTERY_QUICK_SAMPLE_COUNT);
}

static battery_sample_t sample_battery_stable(void)
{
    return sample_battery_window(BATTERY_STABLE_SETTLE_MS, BATTERY_STABLE_DISCARD_SAMPLES, BATTERY_STABLE_SAMPLE_COUNT);
}

static void init_rtc_state(void)
{
    if (s_rtc_state.magic != RTC_STATE_MAGIC)
    {
        memset(&s_rtc_state, 0, sizeof(s_rtc_state));
        s_rtc_state.magic = RTC_STATE_MAGIC;
    }
}

static const char *app_error_text(app_error_t error)
{
    switch (error)
    {
        case APP_ERR_NONE:
            return "";
        case APP_ERR_RGB_INIT:
            return "RGB_INIT";
        case APP_ERR_RECORD_BUFFER:
            return "RECORD_BUFFER";
        case APP_ERR_AUDIO_INIT:
            return "AUDIO_INIT";
        case APP_ERR_RECORD_TASK:
            return "RECORD_TASK";
        case APP_ERR_WRITE_TASK:
            return "WRITE_TASK";
        case APP_ERR_NO_SD:
            return "NO_SD";
        case APP_ERR_SD_MOUNT:
            return "SD_MOUNT";
        case APP_ERR_RECORD_OPEN:
            return "RECORD_OPEN";
        case APP_ERR_LOG_OPEN:
            return "LOG_OPEN";
        case APP_ERR_DISPLAY_INIT:
            return "DISPLAY_INIT";
        case APP_ERR_AUDIO_READ:
            return "AUDIO_READ";
        case APP_ERR_SD_WRITE:
            return "SD_WRITE";
        case APP_ERR_LOG_WRITE:
            return "LOG_WRITE";
        default:
            return "UNKNOWN";
    }
}

static esp_err_t reserve_record_path(char *path_buffer, size_t path_size, char *name_buffer, size_t name_size, uint32_t *seq_out)
{
    init_rtc_state();

    for (uint32_t attempts = 0; attempts < 100000UL; ++attempts)
    {
        uint32_t seq = s_rtc_state.next_session_seq++;
        snprintf(name_buffer, name_size, RECORD_NAME_FMT, (unsigned long)seq);
        snprintf(path_buffer, path_size, RECORD_PATH_FMT, (unsigned long)seq);

        FILE *probe = fopen(path_buffer, "rb");
        if (probe == NULL)
        {
            *seq_out = seq;
            return ESP_OK;
        }
        fclose(probe);
    }

    return ESP_FAIL;
}

static uint32_t current_record_duration_ms(void)
{
    if (s_record_start_us <= 0)
    {
        return 0;
    }

    int64_t now_us = esp_timer_get_time();
    if (now_us <= s_record_start_us)
    {
        return 0;
    }

    return (uint32_t)((now_us - s_record_start_us) / 1000LL);
}

static esp_err_t open_log_file(void)
{
    s_log_file = fopen(LOG_PATH, "a+");
    if (s_log_file == NULL)
    {
        return ESP_FAIL;
    }

    setvbuf(s_log_file, NULL, _IOFBF, 512);
    fseek(s_log_file, 0, SEEK_END);
    long current_size = ftell(s_log_file);
    if (current_size == 0)
    {
        if (fprintf(s_log_file, "seq,rtc_us,uptime_ms,event,wakeup_cause,file_name,bytes_written,duration_ms,battery_mv,battery_percent,error_code\n") < 0)
        {
            fclose(s_log_file);
            s_log_file = NULL;
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static void close_log_file(void)
{
    if (s_log_file == NULL)
    {
        return;
    }

    fflush(s_log_file);
    (void)fsync(fileno(s_log_file));
    fclose(s_log_file);
    s_log_file = NULL;
}

static esp_err_t log_event_csv(const char *event, size_t bytes_written, const battery_sample_t *sample, app_error_t error_code, bool flush_now, int32_t duration_override_ms)
{
    if (s_log_file == NULL)
    {
        return ESP_FAIL;
    }

    uint32_t duration_ms = duration_override_ms >= 0 ? (uint32_t)duration_override_ms : current_record_duration_ms();
    const char *file_name = s_record_name[0] != '\0' ? s_record_name : "";
    const char *error_text = app_error_text(error_code);

    int rc = fprintf(s_log_file,
        "%lu,%llu,%llu,%s,%d,%s,%llu,%lu,",
        (unsigned long)s_session_seq,
        (unsigned long long)esp_clk_rtc_time(),
        (unsigned long long)(esp_timer_get_time() / 1000LL),
        event,
        s_wakeup_cause,
        file_name,
        (unsigned long long)bytes_written,
        (unsigned long)duration_ms);
    if (rc < 0)
    {
        return ESP_FAIL;
    }

    if ((sample != NULL) && sample->valid)
    {
        rc = fprintf(s_log_file, "%u,%.2f,", sample->voltage_mv, (double)sample->percent_est);
    }
    else
    {
        rc = fprintf(s_log_file, ",,");
    }
    if (rc < 0)
    {
        return ESP_FAIL;
    }

    if (fprintf(s_log_file, "%s\n", error_text) < 0)
    {
        return ESP_FAIL;
    }

    if (flush_now)
    {
        fflush(s_log_file);
        if (fsync(fileno(s_log_file)) != 0)
        {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static const char *ui_state_text(ui_state_t state)
{
    switch (state)
    {
        case UI_STATE_WAKING:
            return "Waking";
        case UI_STATE_RECORDING:
            return "Recording";
        case UI_STATE_SAVING:
            return "Saving...";
        case UI_STATE_ERROR:
            return "Error";
        default:
            return "";
    }
}

static uint32_t estimate_charge_eta_minutes(const battery_sample_t *battery_sample)
{
    if ((battery_sample == NULL) || !battery_sample->valid)
    {
        return 0;
    }

    float remaining_percent = 100.0f - battery_sample->percent_est;
    if (remaining_percent <= 0.5f)
    {
        return 0;
    }

    return (uint32_t)ceilf(remaining_percent * CHARGE_ETA_MINUTES_PER_PERCENT);
}

static void format_eta_text(uint32_t total_minutes, char *buffer, size_t buffer_size)
{
    uint32_t hours = total_minutes / 60U;
    uint32_t minutes = total_minutes % 60U;
    snprintf(buffer, buffer_size, "ETA %02lu:%02lu", (unsigned long)hours, (unsigned long)minutes);
}

static void format_duration_text(uint32_t duration_ms, char *buffer, size_t buffer_size)
{
    uint32_t total_sec = duration_ms / 1000U;
    uint32_t total_min = total_sec / 60U;
    uint32_t seconds = total_sec % 60U;
    snprintf(buffer, buffer_size, "%02lu:%02lu", (unsigned long)total_min, (unsigned long)seconds);
}

static void create_ui_locked(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    s_battery_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_battery_label, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_MID, 0, 28);

    s_duration_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_duration_label, lv_color_white(), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(s_duration_label, &lv_font_montserrat_48, LV_PART_MAIN);
#endif
    lv_obj_align(s_duration_label, LV_ALIGN_CENTER, 0, -8);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xA0A0A0), LV_PART_MAIN);
#if CONFIG_LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -32);
}

static void update_ui_locked(const battery_sample_t *battery_sample, const char *center_text, const char *status_text)
{
    char battery_text[32] = { 0 };

    if ((battery_sample != NULL) && battery_sample->valid)
    {
        snprintf(battery_text, sizeof(battery_text), "%u%% / %.3fV", battery_sample->percent_rounded, (double)battery_sample->voltage_mv / 1000.0);
    }
    else
    {
        snprintf(battery_text, sizeof(battery_text), "--%% / -.---V");
    }

    lv_label_set_text(s_battery_label, battery_text);
    lv_label_set_text(s_duration_label, center_text);
    lv_label_set_text(s_status_label, status_text);
}

static void render_ui_text(const battery_sample_t *battery_sample, const char *center_text, const char *status_text)
{
    if (!s_display_ready)
    {
        return;
    }

    if (!lvgl_port_lock(1000))
    {
        ESP_LOGW(TAG, "Failed to lock LVGL");
        return;
    }

    if (s_battery_label == NULL)
    {
        create_ui_locked();
    }

    update_ui_locked(battery_sample, center_text, status_text);
    lv_refr_now(NULL);
    lvgl_port_unlock();
}

static void render_record_ui(const battery_sample_t *battery_sample, ui_state_t state, uint32_t duration_ms)
{
    char duration_text[16] = { 0 };
    format_duration_text(duration_ms, duration_text, sizeof(duration_text));
    render_ui_text(battery_sample, duration_text, ui_state_text(state));
}

static esp_err_t ensure_display_ready(void)
{
    if (!s_display_ready)
    {
        if (bsp_lvgl_init() == NULL)
        {
            return ESP_FAIL;
        }
        s_display_ready = true;
    }

    return bsp_lcd_brightness_set(APP_BRIGHTNESS_PERCENT);
}

static void render_usb_mode_ui(const battery_sample_t *battery_sample, bool debug_mode, bool charging, bool charge_full)
{
    char status_text[32] = { 0 };

    if (debug_mode)
    {
        snprintf(status_text, sizeof(status_text), "Press knob to record");
        render_ui_text(battery_sample, "Debug Mode", status_text);
        return;
    }

    if (charge_full)
    {
        snprintf(status_text, sizeof(status_text), "Fully charged");
        render_ui_text(battery_sample, "Charged", status_text);
        return;
    }

    if (charging)
    {
        format_eta_text(estimate_charge_eta_minutes(battery_sample), status_text, sizeof(status_text));
        render_ui_text(battery_sample, "Charging", status_text);
        return;
    }

    snprintf(status_text, sizeof(status_text), "Press knob to record");
    render_ui_text(battery_sample, "USB Power", status_text);
}

static bool run_usb_mode_loop(void)
{
    int64_t last_status_refresh_us = 0;
    int64_t last_battery_refresh_us = 0;
    bool previous_pressed = is_button_pressed();

    if (ensure_display_ready() != ESP_OK)
    {
        return false;
    }

    s_display_battery = sample_battery_quick();

    while (true)
    {
        bool vbus_present = is_vbus_present();
        if (!vbus_present)
        {
            set_rtc_vbus_present(false);
            disable_display_for_sleep();
            enter_deep_sleep_now();
            return false;
        }

        int64_t now_us = esp_timer_get_time();
        if (((now_us - last_status_refresh_us) >= (USB_STATUS_REFRESH_MS * 1000LL)) || (last_status_refresh_us == 0))
        {
            bool debug_mode = is_usb_data_connected();
            bool charging = bsp_system_is_charging();
            bool charge_full = bsp_system_is_standby();
            render_usb_mode_ui(&s_display_battery, debug_mode, charging, charge_full);
            last_status_refresh_us = now_us;
        }

        if (((now_us - last_battery_refresh_us) >= (USB_BATTERY_REFRESH_MS * 1000LL)) || (last_battery_refresh_us == 0))
        {
            s_display_battery = sample_battery_quick();
            last_battery_refresh_us = now_us;
            continue;
        }

        bool pressed = is_button_pressed();
        if (pressed && !previous_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (is_button_pressed())
            {
                set_rtc_vbus_present(true);
                wait_for_button_release();
                return true;
            }
        }

        previous_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

static void blank_display_before_sleep(void)
{
    if (!s_display_ready)
    {
        return;
    }

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

static void disable_display_for_sleep(void)
{
    blank_display_before_sleep();
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_lcd_brightness_set(0));
    esp_lcd_panel_handle_t panel = bsp_lcd_get_panel_handle();
    if (panel != NULL)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel, false));
    }
}

static bool led_delay_or_stop(uint32_t total_ms)
{
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < total_ms)
    {
        if (s_led_stop_requested)
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
        elapsed_ms += 20;
    }
    return s_led_stop_requested;
}

static void led_blink_task(void *arg)
{
    s_led_task_running = true;

    while (!s_led_stop_requested)
    {
        (void)bsp_rgb_set(LED_WHITE_LEVEL, LED_WHITE_LEVEL, LED_WHITE_LEVEL);
        if (led_delay_or_stop(LED_FLASH_ON_MS))
        {
            break;
        }
        (void)bsp_rgb_set(0, 0, 0);
        if (led_delay_or_stop(LED_FLASH_OFF_MS))
        {
            break;
        }
    }

    (void)bsp_rgb_set(0, 0, 0);
    s_led_task_running = false;
    s_led_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t start_record_led(void)
{
    if (!s_rgb_ready)
    {
        ESP_RETURN_ON_ERROR(bsp_rgb_init(), TAG, "RGB init failed");
        s_rgb_ready = true;
    }

    s_led_stop_requested = false;
    if (xTaskCreate(led_blink_task, "record_led", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, &s_led_task) != pdPASS)
    {
        s_led_task = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void stop_record_led(void)
{
    s_led_stop_requested = true;
    uint32_t waited_ms = 0;
    while (s_led_task_running && (waited_ms < 400U))
    {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited_ms += 20U;
    }

    if (s_rgb_ready)
    {
        (void)bsp_rgb_set(0, 0, 0);
    }
}

static void flash_error_led(void)
{
    if (!s_rgb_ready)
    {
        if (bsp_rgb_init() != ESP_OK)
        {
            return;
        }
        s_rgb_ready = true;
    }

    stop_record_led();

    for (uint8_t index = 0; index < 3; ++index)
    {
        (void)bsp_rgb_set(LED_RED_LEVEL, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(LED_ERROR_BLINK_MS));
        (void)bsp_rgb_set(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(LED_ERROR_BLINK_MS));
    }
}

static void destroy_record_pipe(void)
{
    if (s_record_pipe.free_queue != NULL)
    {
        vQueueDelete(s_record_pipe.free_queue);
    }
    if (s_record_pipe.filled_queue != NULL)
    {
        vQueueDelete(s_record_pipe.filled_queue);
    }
    if (s_record_pipe.storage != NULL)
    {
        free(s_record_pipe.storage);
    }
    memset(&s_record_pipe, 0, sizeof(s_record_pipe));
}

static esp_err_t init_record_pipe(void)
{
    memset(&s_record_pipe, 0, sizeof(s_record_pipe));

    s_record_pipe.storage = heap_caps_malloc(RECORD_BUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_record_pipe.storage == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    s_record_pipe.free_queue = xQueueCreate(RECORD_CHUNK_COUNT, sizeof(uint16_t));
    s_record_pipe.filled_queue = xQueueCreate(RECORD_CHUNK_COUNT + 1U, sizeof(record_chunk_msg_t));
    if ((s_record_pipe.free_queue == NULL) || (s_record_pipe.filled_queue == NULL))
    {
        destroy_record_pipe();
        return ESP_FAIL;
    }

    for (uint16_t index = 0; index < RECORD_CHUNK_COUNT; ++index)
    {
        xQueueSend(s_record_pipe.free_queue, &index, portMAX_DELAY);
    }

    return ESP_OK;
}

static uint8_t *record_chunk_ptr(uint16_t chunk_index)
{
    return s_record_pipe.storage + ((size_t)chunk_index * RECORD_CHUNK_BYTES);
}

static void record_capture_task(void *arg)
{
    while (!s_record_pipe.stop_requested)
    {
        uint16_t chunk_index = 0;
        if (xQueueReceive(s_record_pipe.free_queue, &chunk_index, pdMS_TO_TICKS(RECORD_QUEUE_WAIT_MS)) != pdPASS)
        {
            continue;
        }

        size_t bytes_read = 0;
        esp_err_t ret = bsp_i2s_read(record_chunk_ptr(chunk_index), RECORD_CHUNK_BYTES, &bytes_read, 1000);
        if (ret != ESP_OK)
        {
            s_runtime_error = APP_ERR_AUDIO_READ;
            s_record_pipe.stop_requested = true;
            xQueueSend(s_record_pipe.free_queue, &chunk_index, 0);
            break;
        }

        record_chunk_msg_t message = {
            .chunk_index = chunk_index,
            .bytes = bytes_read,
            .sentinel = false,
        };

        if (xQueueSend(s_record_pipe.filled_queue, &message, pdMS_TO_TICKS(RECORD_QUEUE_WAIT_MS)) != pdPASS)
        {
            s_runtime_error = APP_ERR_AUDIO_READ;
            s_record_pipe.stop_requested = true;
            xQueueSend(s_record_pipe.free_queue, &chunk_index, 0);
            break;
        }
    }

    record_chunk_msg_t sentinel = {
        .chunk_index = UINT16_MAX,
        .bytes = 0,
        .sentinel = true,
    };
    xQueueSend(s_record_pipe.filled_queue, &sentinel, pdMS_TO_TICKS(RECORD_QUEUE_WAIT_MS));

    s_record_pipe.capture_done = true;
    s_record_pipe.capture_task = NULL;
    vTaskDelete(NULL);
}

static void record_writer_task(void *arg)
{
    bool drop_chunks = false;

    while (true)
    {
        record_chunk_msg_t message = { 0 };
        if (xQueueReceive(s_record_pipe.filled_queue, &message, pdMS_TO_TICKS(200)) != pdPASS)
        {
            if (s_record_pipe.capture_done)
            {
                break;
            }
            continue;
        }

        if (message.sentinel)
        {
            break;
        }

        if (!drop_chunks)
        {
            size_t written = fwrite(record_chunk_ptr(message.chunk_index), 1, message.bytes, s_record_file);
            if (written != message.bytes)
            {
                s_runtime_error = APP_ERR_SD_WRITE;
                s_record_pipe.stop_requested = true;
                drop_chunks = true;
            }
            else
            {
                s_record_pipe.bytes_written += written;
            }
        }

        xQueueSend(s_record_pipe.free_queue, &message.chunk_index, portMAX_DELAY);
    }

    s_record_pipe.writer_done = true;
    s_record_pipe.writer_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t start_capture_task(void)
{
    if (xTaskCreate(record_capture_task, "record_capture", RECORD_TASK_STACK_SIZE, NULL, RECORD_TASK_PRIORITY, &s_record_pipe.capture_task) != pdPASS)
    {
        s_record_pipe.capture_task = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t start_writer_task(void)
{
    if (xTaskCreate(record_writer_task, "record_writer", WRITER_TASK_STACK_SIZE, NULL, WRITER_TASK_PRIORITY, &s_record_pipe.writer_task) != pdPASS)
    {
        s_record_pipe.writer_task = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void wait_for_record_tasks(uint32_t timeout_ms)
{
    uint32_t waited_ms = 0;
    while (((s_record_pipe.capture_task != NULL) || (s_record_pipe.writer_task != NULL)) && (waited_ms < timeout_ms))
    {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited_ms += 20;
    }
}

static void enter_deep_sleep_now(void)
{
    wait_for_button_release();
    force_optional_power_domains_off(false, false);
    bsp_system_deep_sleep(0);
}

void app_main(void)
{
    app_error_t error = APP_ERR_NONE;
    bool record_file_should_delete = false;
    bool stop_button_armed = false;
    bool previous_pressed = false;
    bool enter_usb_mode = false;
    bool vbus_present = false;
    bool previous_vbus_present = false;
    bool usb_mode_requested_after_record = false;
    int64_t last_ui_refresh_us = 0;
    int64_t last_usb_refresh_us = 0;
    battery_sample_t stable_battery = { 0 };

    ESP_LOGW(TAG, "Recorder firmware start");

    s_wakeup_cause = esp_sleep_get_wakeup_cause();
    init_rtc_state();

    if (bsp_io_expander_init() == NULL)
    {
        ESP_LOGE(TAG, "IO expander init failed");
        abort();
    }

    ESP_ERROR_CHECK(force_optional_power_domains_off(false, false));

    vbus_present = is_vbus_present();
    previous_vbus_present = was_vbus_present_before_boot();
    set_rtc_vbus_present(vbus_present);

    if (s_wakeup_cause != ESP_SLEEP_WAKEUP_EXT0)
    {
        if (!vbus_present)
        {
            ESP_LOGW(TAG, "Cold boot, entering deep sleep");
            set_rtc_vbus_present(false);
            wait_for_button_release();
            enter_deep_sleep_now();
            return;
        }

        enter_usb_mode = true;
    }
    else if (vbus_present && !previous_vbus_present && !is_button_pressed())
    {
        ESP_LOGW(TAG, "VBUS wake detected");
        enter_usb_mode = true;
    }

usb_mode_start:
    if (enter_usb_mode)
    {
        set_rtc_vbus_present(true);
        if (!run_usb_mode_loop())
        {
            return;
        }

        enter_usb_mode = false;
        vbus_present = is_vbus_present();
    }

    error = APP_ERR_NONE;
    record_file_should_delete = false;
    stop_button_armed = false;
    previous_pressed = false;
    usb_mode_requested_after_record = vbus_present;
    last_ui_refresh_us = 0;
    last_usb_refresh_us = 0;
    stable_battery = (battery_sample_t){ 0 };

    s_runtime_error = APP_ERR_NONE;
    s_record_pipe.stop_requested = false;
    s_record_pipe.capture_done = false;
    s_record_pipe.writer_done = false;
    s_record_pipe.bytes_written = 0;
    s_record_file = NULL;
    s_log_file = NULL;
    s_record_path[0] = '\0';
    s_record_name[0] = '\0';
    s_audio_started = false;
    s_sd_powered = false;
    s_sd_mounted = false;
    s_record_start_us = 0;

    if (start_record_led() != ESP_OK)
    {
        error = APP_ERR_RGB_INIT;
        goto fail;
    }

    if (init_record_pipe() != ESP_OK)
    {
        error = APP_ERR_RECORD_BUFFER;
        goto fail;
    }

    if (bsp_codec_init() != ESP_OK)
    {
        error = APP_ERR_AUDIO_INIT;
        goto fail;
    }
    s_audio_started = true;

    s_record_start_us = esp_timer_get_time();
    if (start_capture_task() != ESP_OK)
    {
        error = APP_ERR_RECORD_TASK;
        goto fail;
    }

    if (!bsp_sdcard_is_inserted())
    {
        error = APP_ERR_NO_SD;
        goto fail;
    }

    if (set_sd_power(true) != ESP_OK)
    {
        error = APP_ERR_SD_MOUNT;
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    if (bsp_sdcard_init_default() != ESP_OK)
    {
        error = APP_ERR_SD_MOUNT;
        goto fail;
    }
    s_sd_mounted = true;

    if (reserve_record_path(s_record_path, sizeof(s_record_path), s_record_name, sizeof(s_record_name), &s_session_seq) != ESP_OK)
    {
        error = APP_ERR_RECORD_OPEN;
        goto fail;
    }

    s_record_file = fopen(s_record_path, "wb");
    if (s_record_file == NULL)
    {
        error = APP_ERR_RECORD_OPEN;
        goto fail;
    }
    setvbuf(s_record_file, NULL, _IOFBF, 8192);
    record_file_should_delete = true;

    if (open_log_file() != ESP_OK)
    {
        error = APP_ERR_LOG_OPEN;
        goto fail;
    }

    if (start_writer_task() != ESP_OK)
    {
        error = APP_ERR_WRITE_TASK;
        goto fail;
    }

    if (log_event_csv("WAKE", 0, NULL, APP_ERR_NONE, false, 0) != ESP_OK)
    {
        error = APP_ERR_LOG_WRITE;
        goto fail;
    }
    if (log_event_csv("REC_START", 0, NULL, APP_ERR_NONE, true, -1) != ESP_OK)
    {
        error = APP_ERR_LOG_WRITE;
        goto fail;
    }

    if (ensure_display_ready() != ESP_OK)
    {
        error = APP_ERR_DISPLAY_INIT;
        goto fail;
    }
    render_record_ui(NULL, UI_STATE_WAKING, 0);
    ESP_LOGW(TAG, "DISPLAY_READY");

    s_display_battery = sample_battery_quick();
    render_record_ui(&s_display_battery, UI_STATE_RECORDING, current_record_duration_ms());

    previous_pressed = is_button_pressed();
    last_ui_refresh_us = esp_timer_get_time();
    last_usb_refresh_us = last_ui_refresh_us;

    while (!s_record_pipe.stop_requested)
    {
        if (s_runtime_error != APP_ERR_NONE)
        {
            error = s_runtime_error;
            goto fail;
        }

        bool pressed = is_button_pressed();
        if (!stop_button_armed)
        {
            if (!pressed)
            {
                stop_button_armed = true;
                previous_pressed = false;
            }
        }
        else if (pressed && !previous_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (is_button_pressed())
            {
                if (log_event_csv("REC_STOP_REQ", s_record_pipe.bytes_written, NULL, APP_ERR_NONE, false, -1) != ESP_OK)
                {
                    error = APP_ERR_LOG_WRITE;
                    goto fail;
                }

                render_record_ui(&s_display_battery, UI_STATE_SAVING, current_record_duration_ms());
                if (log_event_csv("REC_SAVING", s_record_pipe.bytes_written, NULL, APP_ERR_NONE, true, -1) != ESP_OK)
                {
                    error = APP_ERR_LOG_WRITE;
                    goto fail;
                }

                s_record_pipe.stop_requested = true;
                break;
            }
        }

        int64_t now_us = esp_timer_get_time();
        if ((now_us - last_ui_refresh_us) >= (UI_REFRESH_MS * 1000LL))
        {
            render_record_ui(&s_display_battery, UI_STATE_RECORDING, current_record_duration_ms());
            last_ui_refresh_us = now_us;
        }

        if (!usb_mode_requested_after_record && ((now_us - last_usb_refresh_us) >= (USB_STATUS_REFRESH_MS * 1000LL)))
        {
            usb_mode_requested_after_record = is_vbus_present();
            last_usb_refresh_us = now_us;
            if (usb_mode_requested_after_record)
            {
                set_rtc_vbus_present(true);
            }
        }

        previous_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }

    wait_for_record_tasks(5000);
    if (s_runtime_error != APP_ERR_NONE)
    {
        error = s_runtime_error;
        goto fail;
    }

    if (fflush(s_record_file) != 0)
    {
        error = APP_ERR_SD_WRITE;
        goto fail;
    }
    if (fsync(fileno(s_record_file)) != 0)
    {
        error = APP_ERR_SD_WRITE;
        goto fail;
    }
    if (fclose(s_record_file) != 0)
    {
        s_record_file = NULL;
        error = APP_ERR_SD_WRITE;
        goto fail;
    }
    s_record_file = NULL;
    record_file_should_delete = false;

    if (log_event_csv("REC_SAVED", s_record_pipe.bytes_written, NULL, APP_ERR_NONE, false, -1) != ESP_OK)
    {
        error = APP_ERR_LOG_WRITE;
        goto fail;
    }

    stop_record_led();
    if (s_audio_started)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_codec_dev_stop());
        s_audio_started = false;
    }

    close_log_file();

    if (s_sd_mounted)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_sdcard_deinit_default());
        s_sd_mounted = false;
    }
    if (s_sd_powered)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(set_sd_power(false));
    }

    destroy_record_pipe();

    if (usb_mode_requested_after_record || is_vbus_present())
    {
        wait_for_button_release();
        enter_usb_mode = true;
        goto usb_mode_start;
    }

    set_rtc_vbus_present(false);
    disable_display_for_sleep();
    ESP_ERROR_CHECK(force_optional_power_domains_off(false, false));
    vTaskDelay(pdMS_TO_TICKS(BATTERY_LCD_OFF_SETTLE_MS));

    stable_battery = sample_battery_stable();
    if (open_log_file() != ESP_OK)
    {
        error = APP_ERR_LOG_OPEN;
        goto fail;
    }
    if (log_event_csv("PRE_SLEEP_SAMPLE", s_record_pipe.bytes_written, &stable_battery, APP_ERR_NONE, false, -1) != ESP_OK)
    {
        error = APP_ERR_LOG_WRITE;
        goto fail;
    }
    if (log_event_csv("SLEEP_ENTER", s_record_pipe.bytes_written, &stable_battery, APP_ERR_NONE, true, -1) != ESP_OK)
    {
        error = APP_ERR_LOG_WRITE;
        goto fail;
    }

    close_log_file();
    enter_deep_sleep_now();
    return;

fail:
    ESP_LOGE(TAG, "Fatal error: %s", app_error_text(error));

    if (s_display_ready)
    {
        render_ui_text(&s_display_battery, "Error", app_error_text(error));
    }

    s_record_pipe.stop_requested = true;
    wait_for_record_tasks(5000);

    if (s_log_file != NULL)
    {
        (void)log_event_csv("ERROR", s_record_pipe.bytes_written, NULL, error, true, -1);
    }

    if (s_audio_started)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_codec_dev_stop());
        s_audio_started = false;
    }

    if (s_record_file != NULL)
    {
        fclose(s_record_file);
        s_record_file = NULL;
    }
    if (record_file_should_delete)
    {
        (void)remove(s_record_path);
    }

    close_log_file();

    if (s_sd_mounted)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_sdcard_deinit_default());
        s_sd_mounted = false;
    }
    if (s_sd_powered)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(set_sd_power(false));
    }

    flash_error_led();
    destroy_record_pipe();

    if (is_vbus_present())
    {
        wait_for_button_release();
        enter_usb_mode = true;
        goto usb_mode_start;
    }

    if (s_display_ready)
    {
        disable_display_for_sleep();
    }

    set_rtc_vbus_present(false);
    enter_deep_sleep_now();
}
