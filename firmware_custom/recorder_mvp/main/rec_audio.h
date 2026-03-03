#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REC_AUDIO_SAMPLE_RATE      16000
#define REC_AUDIO_CHANNELS         1
#define REC_AUDIO_BITS_PER_SAMPLE  16
#define REC_AUDIO_FRAME_SAMPLES    320
#define REC_AUDIO_SPECTRUM_BINS    96

typedef struct {
    uint16_t rms;
    uint16_t peak;
} rec_audio_levels_t;

typedef void (*rec_audio_pcm_cb_t)(const int16_t *samples, size_t sample_count, void *user_ctx);

esp_err_t rec_audio_init(void);
void rec_audio_set_pcm_callback(rec_audio_pcm_cb_t cb, void *user_ctx);
esp_err_t rec_audio_start(void);
esp_err_t rec_audio_stop(void);
bool rec_audio_is_running(void);
void rec_audio_get_levels(rec_audio_levels_t *out_levels);
void rec_audio_get_spectrum(uint8_t out_levels[REC_AUDIO_SPECTRUM_BINS]);

#ifdef __cplusplus
}
#endif