#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensecap-watcher.h"
#include "rec_audio.h"

static const char *TAG = "rec_audio";

static TaskHandle_t s_audio_task;
static volatile bool s_running;
static rec_audio_levels_t s_levels;
static uint8_t s_spectrum[REC_AUDIO_SPECTRUM_BINS];
static rec_audio_pcm_cb_t s_pcm_cb;
static void *s_pcm_cb_ctx;

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
    *out_levels = s_levels;
}

void rec_audio_get_spectrum(uint8_t out_levels[REC_AUDIO_SPECTRUM_BINS])
{
    if (!out_levels) {
        return;
    }
    memcpy(out_levels, s_spectrum, REC_AUDIO_SPECTRUM_BINS);
}
