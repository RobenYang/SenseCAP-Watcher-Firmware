#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REC_STORAGE_MOUNT_PATH      "/sdcard"
#define REC_STORAGE_RECORDINGS_DIR  "/sdcard/recordings"
#define REC_STORAGE_SEGMENT_SECONDS (30 * 60)

esp_err_t rec_storage_init(void);
bool rec_storage_is_usb_attached(void);
bool rec_storage_is_usb_exposed(void);
bool rec_storage_is_recording(void);
bool rec_storage_has_error(void);

esp_err_t rec_storage_start_recording(void);
esp_err_t rec_storage_stop_recording(void);
void rec_storage_pcm_sink(const int16_t *samples, size_t sample_count, void *user_ctx);

esp_err_t rec_storage_enter_usb_exposed(void);
esp_err_t rec_storage_exit_usb_exposed(void);

#ifdef __cplusplus
}
#endif
