#include "rec_ui.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "esp_lvgl_port.h"

#include "sensecap-watcher.h"

#define REC_UI_OUTER_MARGIN        8
#define REC_UI_HOLE_RADIUS         86
#define REC_UI_BAR_MIN_LEN         8
#define REC_UI_BAR_WIDTH_MAIN      3
#define REC_UI_BAR_WIDTH_TRAIL     2
#define REC_UI_Q15_SCALE           32767
#define REC_UI_ATTACK_NUM          6
#define REC_UI_ATTACK_DEN          10
#define REC_UI_RELEASE_NUM         2
#define REC_UI_RELEASE_DEN         10

static lv_obj_t *s_root;
static lv_obj_t *s_ring;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_status_label;
static bool s_recording;

static uint8_t s_target_bins[REC_AUDIO_SPECTRUM_BINS];
static uint8_t s_frame_bins[REC_AUDIO_SPECTRUM_BINS];
static uint8_t s_prev_bins_1[REC_AUDIO_SPECTRUM_BINS];
static uint8_t s_prev_bins_2[REC_AUDIO_SPECTRUM_BINS];
static int16_t s_cos_q15[REC_AUDIO_SPECTRUM_BINS];
static int16_t s_sin_q15[REC_AUDIO_SPECTRUM_BINS];
static uint8_t s_last_battery = 0xFF;
static char s_last_status[24];

static inline int rec_ui_min(int a, int b)
{
    return (a < b) ? a : b;
}

static void rec_ui_build_lut(void)
{
    const float pi = 3.14159265358979323846f;
    for (int i = 0; i < REC_AUDIO_SPECTRUM_BINS; ++i) {
        float theta = (2.0f * pi * (float)i) / (float)REC_AUDIO_SPECTRUM_BINS;
        s_cos_q15[i] = (int16_t)lrintf(cosf(theta) * (float)REC_UI_Q15_SCALE);
        s_sin_q15[i] = (int16_t)lrintf(sinf(theta) * (float)REC_UI_Q15_SCALE);
    }
}

static void rec_ui_draw_layer(lv_draw_ctx_t *draw_ctx,
                              int cx,
                              int cy,
                              int outer_radius,
                              int hole_radius,
                              const uint8_t bins[REC_AUDIO_SPECTRUM_BINS],
                              lv_color_t color,
                              lv_opa_t opa,
                              lv_coord_t width)
{
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.opa = opa;
    line_dsc.width = width;
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;

    int max_bar_len = outer_radius - hole_radius - 4;
    if (max_bar_len < REC_UI_BAR_MIN_LEN) {
        max_bar_len = REC_UI_BAR_MIN_LEN;
    }

    for (int i = 0; i < REC_AUDIO_SPECTRUM_BINS; ++i) {
        int bar_len = REC_UI_BAR_MIN_LEN + (bins[i] * max_bar_len) / 100;
        int inner_radius = outer_radius - bar_len;
        if (inner_radius < (hole_radius + 2)) {
            inner_radius = hole_radius + 2;
        }

        lv_point_t p1 = {
            .x = (lv_coord_t)(cx + (outer_radius * s_cos_q15[i]) / REC_UI_Q15_SCALE),
            .y = (lv_coord_t)(cy + (outer_radius * s_sin_q15[i]) / REC_UI_Q15_SCALE),
        };
        lv_point_t p2 = {
            .x = (lv_coord_t)(cx + (inner_radius * s_cos_q15[i]) / REC_UI_Q15_SCALE),
            .y = (lv_coord_t)(cy + (inner_radius * s_sin_q15[i]) / REC_UI_Q15_SCALE),
        };
        lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
    }
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

    int outer_radius = rec_ui_min(w, h) / 2 - REC_UI_OUTER_MARGIN;
    int hole_radius = REC_UI_HOLE_RADIUS;

    rec_ui_draw_layer(draw_ctx, cx, cy, outer_radius, hole_radius,
                      s_prev_bins_2, lv_color_hex(0x222222), LV_OPA_70, REC_UI_BAR_WIDTH_TRAIL);
    rec_ui_draw_layer(draw_ctx, cx, cy, outer_radius, hole_radius,
                      s_prev_bins_1, lv_color_hex(0x555555), LV_OPA_COVER, REC_UI_BAR_WIDTH_TRAIL);
    rec_ui_draw_layer(draw_ctx, cx, cy, outer_radius, hole_radius,
                      s_frame_bins, lv_color_white(), LV_OPA_COVER, REC_UI_BAR_WIDTH_MAIN);

    lv_draw_rect_dsc_t hole_dsc;
    lv_draw_rect_dsc_init(&hole_dsc);
    hole_dsc.bg_color = lv_color_black();
    hole_dsc.bg_opa = LV_OPA_COVER;
    hole_dsc.radius = LV_RADIUS_CIRCLE;
    hole_dsc.border_opa = LV_OPA_TRANSP;
    hole_dsc.outline_opa = LV_OPA_TRANSP;

    lv_area_t hole_area = {
        .x1 = (lv_coord_t)(cx - hole_radius),
        .y1 = (lv_coord_t)(cy - hole_radius),
        .x2 = (lv_coord_t)(cx + hole_radius),
        .y2 = (lv_coord_t)(cy + hole_radius),
    };
    lv_draw_rect(draw_ctx, &hole_dsc, &hole_area);
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
    lv_label_set_text(s_status_label, "BOOT");
    lv_obj_align_to(s_status_label, s_battery_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_root);

    rec_ui_build_lut();
    memset(s_target_bins, 0, sizeof(s_target_bins));
    memset(s_frame_bins, 0, sizeof(s_frame_bins));
    memset(s_prev_bins_1, 0, sizeof(s_prev_bins_1));
    memset(s_prev_bins_2, 0, sizeof(s_prev_bins_2));
    memset(s_last_status, 0, sizeof(s_last_status));
    s_recording = false;

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
        lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    } else {
        memset(s_target_bins, 0, sizeof(s_target_bins));
        memset(s_frame_bins, 0, sizeof(s_frame_bins));
        memset(s_prev_bins_1, 0, sizeof(s_prev_bins_1));
        memset(s_prev_bins_2, 0, sizeof(s_prev_bins_2));
        lv_obj_add_flag(s_ring, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_invalidate(s_root);
    lvgl_port_unlock();
}

void rec_ui_set_spectrum(const uint8_t bins[REC_AUDIO_SPECTRUM_BINS])
{
    if (bins == NULL) {
        return;
    }
    memcpy(s_target_bins, bins, REC_AUDIO_SPECTRUM_BINS);
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

    for (int i = 0; i < REC_AUDIO_SPECTRUM_BINS; ++i) {
        uint8_t prev = s_frame_bins[i];
        int diff = (int)s_target_bins[i] - (int)prev;
        if (diff > 0) {
            prev = (uint8_t)(prev + (diff * REC_UI_ATTACK_NUM) / REC_UI_ATTACK_DEN);
        } else {
            prev = (uint8_t)(prev + (diff * REC_UI_RELEASE_NUM) / REC_UI_RELEASE_DEN);
        }

        s_prev_bins_2[i] = s_prev_bins_1[i];
        s_prev_bins_1[i] = s_frame_bins[i];
        s_frame_bins[i] = prev;
    }

    if (!lvgl_port_lock(0)) {
        return;
    }
    lv_obj_invalidate(s_ring);
    lvgl_port_unlock();
}
