#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rec_input_init(void);
void rec_input_update(void);
bool rec_input_take_short_press(void);
bool rec_input_take_long_press(void);

#ifdef __cplusplus
}
#endif
