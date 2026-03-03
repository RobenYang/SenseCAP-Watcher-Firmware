#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensecap-watcher.h"
#include "rec_audio.h"

#define REC_AUDIO_HISTORY_SEGMENT  16
#define REC_AUDIO_HISTORY_SAMPLES  (REC_AUDIO_SPECTRUM_BINS * REC_AUDIO_HISTORY_SEGMENT)

static const char *TAG = "rec_audio";

static TaskHandle_t s_audio_task;
static volatile bool s_running;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static rec_audio_levels_t s_levels;
static uint8_t s_spectrum[REC_AUDIO_SPECTRUM_BINS];
static int16_t s_history[REC_AUDIO_HISTORY_SAMPLES];
static rec_audio_pcm_cb_t s_pcm_cb;
static void *s_pcm_cb_ctx;

static inline uint16_t abs16(int16_t v)
{
    int32_t x = v;
    if (x < 0) {
        x = -x;
    }
    if (x > 32767) {
        x = 32767;
    }
    return (uint16_t)x;
}

static void rec_audio_compute_spectrum_locked(void)
{
    for (size_t bin = 0; bin < REC_AUDIO_SPECTRUM_BINS; ++bin) {
        uint16_t max_abs = 0;
        size_t base = bin * REC_AUDIO_HISTORY_SEGMENT;
        for (size_t i = 0; i < REC_AUDIO_HISTORY_SEGMENT; ++i) {
            uint16_t a = abs16(s_history[base + i]);
            if (a > max_abs) {
                max_abs = a;
            }
        }

        int32_t raw = (int32_t)max_abs - 300;
        if (raw < 0) {
            raw = 0;
        }
        int32_t target = (raw * 100) / 12000;
        if (target > 100) {
            target = 100;
        }

        int32_t prev = s_spectrum[bin];
        int32_t step = target - prev;
        if (step > 0) {
            prev += (step * 6) / 10;
        } else {
            prev += (step * 2) / 10;
        }
        if (prev < 0) {
            prev = 0;
        } else if (prev > 100) {
            prev = 100;
        }
        s_spectrum[bin] = (uint8_t)prev;
    }
}

static void rec_audio_task(void *arg)
{
    (void)arg;

    int16_t frame[REC_AUDIO_FRAME_SAMPLES];
    const size_t frame_bytes = sizeof(frame);

    while (s_running) {
        size_t bytes_read = 0;
        esp_err_t ret = bsp_i2s_read(frame, frame_bytes, &bytes_read, 100);
        if (ret != ESP_OK || bytes_read != frame_bytes) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        uint64_t sum_sq = 0;
        uint16_t peak = 0;
        for (size_t i = 0; i < REC_AUDIO_FRAME_SAMPLES; ++i) {
            uint16_t a = abs16(frame[i]);
            if (a > peak) {
                peak = a;
            }
            sum_sq += (uint64_t)a * (uint64_t)a;
        }
        uint16_t rms = (uint16_t)sqrtf((float)sum_sq / (float)REC_AUDIO_FRAME_SAMPLES);

        portENTER_CRITICAL(&s_lock);
        memmove(s_history, s_history + REC_AUDIO_FRAME_SAMPLES,
                (REC_AUDIO_HISTORY_SAMPLES - REC_AUDIO_FRAME_SAMPLES) * sizeof(int16_t));
        memcpy(s_history + (REC_AUDIO_HISTORY_SAMPLES - REC_AUDIO_FRAME_SAMPLES), frame, frame_bytes);
        s_levels.rms = rms;
        s_levels.peak = peak;
        rec_audio_compute_spectrum_locked();
        portEXIT_CRITICAL(&s_lock);

        if (s_pcm_cb) {
            s_pcm_cb(frame, REC_AUDIO_FRAME_SAMPLES, s_pcm_cb_ctx);
        }
    }

    s_audio_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t rec_audio_init(void)
{
    memset(&s_levels, 0, sizeof(s_levels));
    memset(s_spectrum, 0, sizeof(s_spectrum));
    memset(s_history, 0, sizeof(s_history));

    esp_err_t ret = bsp_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_codec_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

void rec_audio_set_pcm_callback(rec_audio_pcm_cb_t cb, void *user_ctx)
{
    s_pcm_cb = cb;
    s_pcm_cb_ctx = user_ctx;
}

esp_err_t rec_audio_start(void)
{
    if (s_running) {
        return ESP_OK;
    }

    (void)bsp_codec_dev_resume();

    s_running = true;
    BaseType_t ok = xTaskCreate(rec_audio_task, "rec_audio", 4096, NULL, 8, &s_audio_task);
    if (ok != pdPASS) {
        s_running = false;
        s_audio_task = NULL;
        ESP_LOGE(TAG, "failed to create rec_audio task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t rec_audio_stop(void)
{
    if (!s_running) {
        (void)bsp_codec_dev_stop();
        return ESP_OK;
    }

    s_running = false;
    while (s_audio_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    (void)bsp_codec_dev_stop();
    return ESP_OK;
}

bool rec_audio_is_running(void)
{
    return s_running;
}

void rec_audio_get_levels(rec_audio_levels_t *out_levels)
{
    if (!out_levels) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    *out_levels = s_levels;
    portEXIT_CRITICAL(&s_lock);
}

void rec_audio_get_spectrum(uint8_t out_levels[REC_AUDIO_SPECTRUM_BINS])
{
    if (!out_levels) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    memcpy(out_levels, s_spectrum, REC_AUDIO_SPECTRUM_BINS);
    portEXIT_CRITICAL(&s_lock);
}
