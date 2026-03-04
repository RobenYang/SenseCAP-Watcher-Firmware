#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "rec_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rec_ui_init(void);
void rec_ui_set_recording(bool recording);
void rec_ui_set_spectrum(const uint8_t bins[REC_AUDIO_SPECTRUM_BINS]);
void rec_ui_set_battery_percent(uint8_t percent);
void rec_ui_set_status_text(const char *text);
void rec_ui_render_frame(void);

#ifdef __cplusplus
}
#endif
