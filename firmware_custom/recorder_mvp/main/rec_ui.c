#include "rec_ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_lvgl_port.h"
#include "esp_timer.h"

#include "sensecap-watcher.h"

#define REC_UI_RING_MARGIN             6
#define REC_UI_RING_WIDTH              8
#define REC_UI_RING_OPA_MIN            0
#define REC_UI_RING_OPA_MAX            255
#define REC_UI_FADE_WHITE_TO_BLACK_MS  3000
#define REC_UI_HOLD_BLACK_MS           120
#define REC_UI_FADE_BLACK_TO_WHITE_MS  3000
#define REC_UI_HOLD_WHITE_MS           520

static lv_obj_t *s_root;
static lv_obj_t *s_ring;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_status_label;
static bool s_recording;

static uint8_t s_last_battery = 0xFF;
static char s_last_status[24];
static lv_opa_t s_ring_opa = REC_UI_RING_OPA_MIN;
typedef enum {
    REC_UI_PHASE_WHITE_TO_BLACK = 0,
    REC_UI_PHASE_HOLD_BLACK,
    REC_UI_PHASE_BLACK_TO_WHITE,
    REC_UI_PHASE_HOLD_WHITE,
} rec_ui_breathe_phase_t;
static rec_ui_breathe_phase_t s_breathe_phase = REC_UI_PHASE_WHITE_TO_BLACK;
static int64_t s_phase_start_ms = 0;

static inline int rec_ui_min(int a, int b)
{
    return (a < b) ? a : b;
}

static void rec_ui_draw_ring_event(lv_event_t *e)
{
    if (!s_recording) {
        return;
    }

    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    if (draw_ctx == NULL) {
        return;
    }

    lv_obj_t *obj = lv_event_get_target(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int w = lv_area_get_width(&coords);
    int h = lv_area_get_height(&coords);
    int cx = (coords.x1 + coords.x2) / 2;
    int cy = (coords.y1 + coords.y2) / 2;

    int outer_radius = rec_ui_min(w, h) / 2 - REC_UI_RING_MARGIN;
    if (outer_radius < 4) {
        return;
    }

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_white();
    arc_dsc.opa = s_ring_opa;
    arc_dsc.width = REC_UI_RING_WIDTH;
    arc_dsc.rounded = 1;

    lv_point_t center = {
        .x = (lv_coord_t)cx,
        .y = (lv_coord_t)cy,
    };
    lv_draw_arc(draw_ctx, &arc_dsc, &center, outer_radius, 0, 359);
}

esp_err_t rec_ui_init(void)
{
    if (bsp_lvgl_get_disp() == NULL) {
        bsp_display_cfg_t cfg = {
            .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
            .buffer_size = DRV_LCD_H_RES * 32,
            .double_buffer = false,
            .flags = {
                .buff_dma = 0,
                .buff_spiram = 1,
            },
        };
        cfg.lvgl_port_cfg.task_priority = CONFIG_LVGL_PORT_TASK_PRIORITY;
        cfg.lvgl_port_cfg.task_affinity = CONFIG_LVGL_PORT_TASK_AFFINITY;
        cfg.lvgl_port_cfg.task_stack = CONFIG_LVGL_PORT_TASK_STACK_SIZE;
        cfg.lvgl_port_cfg.task_max_sleep_ms = CONFIG_LVGL_PORT_TASK_MAX_SLEEP_MS;
        cfg.lvgl_port_cfg.timer_period_ms = CONFIG_LVGL_PORT_TIMER_PERIOD_MS;
        if (bsp_lvgl_init_with_cfg(&cfg) == NULL) {
            return ESP_FAIL;
        }
    }

    if (!lvgl_port_lock(0)) {
        return ESP_ERR_TIMEOUT;
    }

    s_root = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    s_ring = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_ring);
    lv_obj_set_size(s_ring, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_center(s_ring);
    lv_obj_add_event_cb(s_ring, rec_ui_draw_ring_event, LV_EVENT_DRAW_MAIN, NULL);

    s_battery_label = lv_label_create(s_root);
    lv_obj_set_style_text_color(s_battery_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_battery_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_battery_label, "BAT --%");
    lv_obj_center(s_battery_label);

    s_status_label = lv_label_create(s_root);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_status_label, "");
    lv_obj_align_to(s_status_label, s_battery_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_root);

    memset(s_last_status, 0, sizeof(s_last_status));
    s_recording = false;
    s_ring_opa = REC_UI_RING_OPA_MIN;
    s_breathe_phase = REC_UI_PHASE_WHITE_TO_BLACK;
    s_phase_start_ms = esp_timer_get_time() / 1000;

    lvgl_port_unlock();
    return ESP_OK;
}

void rec_ui_set_recording(bool recording)
{
    if (s_ring == NULL || s_root == NULL) {
        s_recording = recording;
        return;
    }

    s_recording = recording;
    if (!lvgl_port_lock(0)) {
        return;
    }

    if (recording) {
        s_ring_opa = REC_UI_RING_OPA_MAX;
        s_breathe_phase = REC_UI_PHASE_WHITE_TO_BLACK;
        s_phase_start_ms = esp_timer_get_time() / 1000;
        lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_invalidate(s_root);
    lvgl_port_unlock();
}

void rec_ui_set_spectrum(const uint8_t bins[REC_AUDIO_SPECTRUM_BINS])
{
    (void)bins;
}

void rec_ui_set_battery_percent(uint8_t percent)
{
    if (s_battery_label == NULL || percent == s_last_battery) {
        return;
    }
    s_last_battery = percent;

    if (!lvgl_port_lock(0)) {
        return;
    }

    char text[16];
    (void)snprintf(text, sizeof(text), "BAT %u%%", percent);
    lv_label_set_text(s_battery_label, text);
    lv_obj_center(s_battery_label);

    lvgl_port_unlock();
}

void rec_ui_set_status_text(const char *text)
{
    if (s_status_label == NULL || text == NULL) {
        return;
    }

    if (strncmp(s_last_status, text, sizeof(s_last_status) - 1) == 0) {
        return;
    }

    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_label_set_text(s_status_label, text);
    lv_obj_align_to(s_status_label, s_battery_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    (void)snprintf(s_last_status, sizeof(s_last_status), "%s", text);
    lvgl_port_unlock();
}

void rec_ui_render_frame(void)
{
    if (!s_recording || s_ring == NULL) {
        return;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;
    int64_t elapsed_ms = now_ms - s_phase_start_ms;
    if (elapsed_ms < 0) {
        elapsed_ms = 0;
    }

    if (s_breathe_phase == REC_UI_PHASE_WHITE_TO_BLACK) {
        if (elapsed_ms >= REC_UI_FADE_WHITE_TO_BLACK_MS) {
            s_ring_opa = REC_UI_RING_OPA_MIN;
            s_breathe_phase = REC_UI_PHASE_HOLD_BLACK;
            s_phase_start_ms = now_ms;
        } else {
            // Accelerating fade-out: starts slow and gets faster near black.
            uint32_t d = (uint32_t)REC_UI_FADE_WHITE_TO_BLACK_MS;
            uint32_t t = (uint32_t)elapsed_ms;
            uint32_t eased = (t * t * 255U) / (d * d);
            if (eased > 255U) {
                eased = 255U;
            }
            s_ring_opa = (lv_opa_t)(255U - eased);
        }
    } else if (s_breathe_phase == REC_UI_PHASE_HOLD_BLACK) {
        s_ring_opa = REC_UI_RING_OPA_MIN;
        if (elapsed_ms >= REC_UI_HOLD_BLACK_MS) {
            s_breathe_phase = REC_UI_PHASE_BLACK_TO_WHITE;
            s_phase_start_ms = now_ms;
        }
    } else if (s_breathe_phase == REC_UI_PHASE_BLACK_TO_WHITE) {
        if (elapsed_ms >= REC_UI_FADE_BLACK_TO_WHITE_MS) {
            s_ring_opa = REC_UI_RING_OPA_MAX;
            s_breathe_phase = REC_UI_PHASE_HOLD_WHITE;
            s_phase_start_ms = now_ms;
        } else {
            // Decelerating fade-in: starts fast and slows near full white.
            uint32_t d = (uint32_t)REC_UI_FADE_BLACK_TO_WHITE_MS;
            uint32_t t = (uint32_t)elapsed_ms;
            uint32_t inv = d - t;
            uint32_t eased = 255U - ((inv * inv * 255U) / (d * d));
            if (eased > 255U) {
                eased = 255U;
            }
            s_ring_opa = (lv_opa_t)eased;
        }
    } else {
        s_ring_opa = REC_UI_RING_OPA_MAX;
        if (elapsed_ms >= REC_UI_HOLD_WHITE_MS) {
            s_breathe_phase = REC_UI_PHASE_WHITE_TO_BLACK;
            s_phase_start_ms = now_ms;
        }
    }

    if (!lvgl_port_lock(0)) {
        return;
    }
    lv_obj_invalidate(s_ring);
    lvgl_port_unlock();
}
