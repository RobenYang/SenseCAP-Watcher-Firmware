#include "rec_state.h"
#include "rec_audio.h"
#include "rec_storage.h"
#include "rec_input.h"
#include "rec_ui.h"
#include "rec_led.h"
#include "rec_power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "sensecap-watcher.h"

#define REC_APP_LOOP_MS           10
#define REC_APP_FRAME_MS          33
#define REC_APP_BATTERY_MS        1000

static const char *TAG = "rec_app";
static rec_state_t s_state = REC_STATE_STANDBY;
static bool s_storage_ready;
static bool s_audio_ready;
static bool s_input_ready;
static bool s_led_ready;
static bool s_ui_ready;

static void rec_app_enter_error(void)
{
    s_state = REC_STATE_ERROR;
    rec_audio_set_pcm_callback(NULL, NULL);
    (void)rec_audio_stop();
    (void)rec_storage_stop_recording();
    rec_led_set_enabled(false);
    rec_ui_set_recording(false);
    if (s_ui_ready) {
        rec_ui_set_status_text("ERROR");
    }
    rec_power_set_recording_mode(false);
    ESP_LOGE(TAG, "entered error state");
}

static esp_err_t rec_app_start_recording(void)
{
    if (!s_storage_ready || !s_audio_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    if (rec_storage_is_usb_exposed()) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = rec_storage_start_recording();
    if (ret != ESP_OK) {
        return ret;
    }

    rec_audio_set_pcm_callback(rec_storage_pcm_sink, NULL);
    ret = rec_audio_start();
    if (ret != ESP_OK) {
        rec_audio_set_pcm_callback(NULL, NULL);
        (void)rec_storage_stop_recording();
        return ret;
    }

    rec_power_set_recording_mode(true);
    rec_ui_set_recording(true);
    rec_led_set_enabled(true);
    if (s_ui_ready) {
        rec_ui_set_status_text("REC");
    }
    s_state = REC_STATE_RECORDING;
    ESP_LOGI(TAG, "recording started");
    return ESP_OK;
}

static esp_err_t rec_app_stop_recording(void)
{
    rec_audio_set_pcm_callback(NULL, NULL);
    (void)rec_audio_stop();

    esp_err_t ret = rec_storage_stop_recording();
    rec_led_set_enabled(false);
    rec_ui_set_recording(false);
    rec_power_set_recording_mode(false);
    if (s_ui_ready) {
        rec_ui_set_status_text("IDLE");
    }
    ESP_LOGI(TAG, "recording stopped");
    return ret;
}

static void rec_app_handle_usb_attach(void)
{
    if (s_state == REC_STATE_RECORDING) {
        if (rec_app_stop_recording() != ESP_OK) {
            rec_app_enter_error();
            return;
        }
    }

    if (s_state == REC_STATE_ERROR) {
        return;
    }

    if (rec_storage_enter_usb_exposed() != ESP_OK) {
        rec_app_enter_error();
        return;
    }

    s_state = REC_STATE_USB_EXPOSED;
    if (s_ui_ready) {
        rec_ui_set_status_text("USB");
    }
    ESP_LOGI(TAG, "usb msc exposed");
}

static void rec_app_handle_usb_detach(void)
{
    if (s_state != REC_STATE_USB_EXPOSED) {
        return;
    }

    if (rec_storage_exit_usb_exposed() != ESP_OK) {
        rec_app_enter_error();
        return;
    }

    s_state = REC_STATE_STANDBY;
    rec_power_set_recording_mode(false);
    if (s_ui_ready) {
        rec_ui_set_status_text("IDLE");
    }
    ESP_LOGI(TAG, "usb detached, back to standby");
}

void app_main(void)
{
    ESP_LOGI(TAG, "recorder_mvp boot");

    if (rec_power_init() != ESP_OK) {
        ESP_LOGE(TAG, "power init failed");
    }

    s_ui_ready = (rec_ui_init() == ESP_OK);
    if (!s_ui_ready) {
        ESP_LOGE(TAG, "ui init failed");
    }

    s_storage_ready = (rec_storage_init() == ESP_OK);
    if (!s_storage_ready) {
        ESP_LOGE(TAG, "storage init failed");
    }

    s_audio_ready = (rec_audio_init() == ESP_OK);
    if (!s_audio_ready) {
        ESP_LOGE(TAG, "audio init failed");
    }

    s_input_ready = (rec_input_init() == ESP_OK);
    if (!s_input_ready) {
        ESP_LOGE(TAG, "input init failed");
    }

    s_led_ready = (rec_led_init() == ESP_OK);
    if (!s_led_ready) {
        ESP_LOGE(TAG, "led init failed");
    }

    rec_power_set_recording_mode(false);
    if (s_ui_ready) {
        rec_ui_set_recording(false);
        rec_ui_set_battery_percent(bsp_battery_get_percent());
        rec_ui_set_status_text(s_storage_ready ? "IDLE" : "SD ERR");
    }
    if (s_led_ready) {
        rec_led_set_enabled(false);
    }

    bool usb_attached_prev = s_storage_ready ? rec_storage_is_usb_attached() : false;
    if (usb_attached_prev) {
        rec_app_handle_usb_attach();
    }

    int64_t last_frame_ms = esp_timer_get_time() / 1000;
    int64_t last_battery_ms = last_frame_ms;

    while (1) {
        if (s_input_ready) {
            rec_input_update();
        }

        if (s_storage_ready) {
            bool usb_attached = rec_storage_is_usb_attached();
            if (usb_attached != usb_attached_prev) {
                usb_attached_prev = usb_attached;
                if (usb_attached) {
                    rec_app_handle_usb_attach();
                } else {
                    rec_app_handle_usb_detach();
                }
            }
        }

        if (s_input_ready && rec_input_take_short_press()) {
            if (s_state == REC_STATE_STANDBY) {
                if (rec_app_start_recording() != ESP_OK) {
                    if (s_ui_ready) {
                        rec_ui_set_status_text(s_storage_ready ? "REC ERR" : "SD ERR");
                    }
                    ESP_LOGW(TAG, "start recording rejected");
                }
            } else if (s_state == REC_STATE_RECORDING) {
                if (rec_app_stop_recording() != ESP_OK) {
                    rec_app_enter_error();
                } else if (usb_attached_prev) {
                    rec_app_handle_usb_attach();
                } else {
                    s_state = REC_STATE_STANDBY;
                }
            } else if (s_state == REC_STATE_ERROR && !usb_attached_prev) {
                s_state = REC_STATE_STANDBY;
            }
        }

        if (s_storage_ready && s_state == REC_STATE_RECORDING && rec_storage_has_error()) {
            rec_app_enter_error();
        }

        int64_t now_ms = esp_timer_get_time() / 1000;
        if (s_ui_ready && (now_ms - last_battery_ms) >= REC_APP_BATTERY_MS) {
            rec_ui_set_battery_percent(bsp_battery_get_percent());
            last_battery_ms = now_ms;
        }

        if (s_state == REC_STATE_RECORDING && (now_ms - last_frame_ms) >= REC_APP_FRAME_MS) {
            uint8_t bins[REC_AUDIO_SPECTRUM_BINS];
            rec_audio_levels_t levels;
            rec_audio_get_levels(&levels);
            rec_audio_get_spectrum(bins);
            if (s_ui_ready) {
                rec_ui_set_spectrum(bins);
                rec_ui_render_frame();
            }
            if (s_led_ready) {
                rec_led_update_rms(levels.rms);
            }
            last_frame_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(REC_APP_LOOP_MS));
    }
}
