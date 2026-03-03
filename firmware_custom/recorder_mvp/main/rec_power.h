#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rec_power_init(void);
void rec_power_set_recording_mode(bool recording_on);
bool rec_power_is_recording_mode(void);

#ifdef __cplusplus
}
#endif
