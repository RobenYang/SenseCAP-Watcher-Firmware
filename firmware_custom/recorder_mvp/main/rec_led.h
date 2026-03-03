#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rec_led_init(void);
void rec_led_set_enabled(bool enabled);
void rec_led_update_rms(uint16_t rms);

#ifdef __cplusplus
}
#endif
