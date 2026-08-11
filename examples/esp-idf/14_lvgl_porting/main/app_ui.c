/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "app_ui.h"

#include <inttypes.h>
#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "app_media.h"

#define UI_UPDATE_PERIOD_MS    40
#define CHART_POINTS           80

#define SCREEN_W               800
#define SCREEN_H               480
#define UI_MARGIN              10
#define UI_GAP                 10
#define HEADER_Y               8
#define HEADER_H               62
#define STRIP_Y                78
#define STRIP_H                40
#define PANEL_Y                126
#define PANEL_H                (SCREEN_H - PANEL_Y - 12)
#define MEDIA_W                340
#define PERF_W                 220
#define CHART_W                200

static const char *TAG = "app_ui";

static lv_obj_t *s_video_canvas;
static lv_obj_t *s_cpu_bar;
static lv_obj_t *s_psram_bar;
static lv_obj_t *s_bandwidth_bar;
static lv_obj_t *s_temp_arc;
static lv_obj_t *s_fps_label;
static lv_obj_t *s_heap_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_mode_label;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_chart_series;
static lv_obj_t *s_meter_arc;
static lv_obj_t *s_photo_image;

static volatile uint32_t s_video_frames;
static volatile uint32_t s_ui_ticks;
static volatile bool s_video_dirty;
static volatile bool s_turbo_mode = true;

static void set_panel_style(lv_obj_t *obj, lv_color_t bg, lv_color_t border)
{
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static lv_obj_t *create_panel_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = create_label(parent, text, &lv_font_montserrat_16, lv_color_hex(0xf2f7fb));
    lv_obj_set_width(label, lv_pct(100));
    return label;
}

static lv_obj_t *create_stat_bar(lv_obj_t *parent, const char *name, lv_color_t color)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), 34);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 3, 0);

    lv_obj_t *label = create_label(row, name, &lv_font_montserrat_14, lv_color_hex(0xb9c7d8));
    lv_obj_set_width(label, lv_pct(100));

    lv_obj_t *bar = lv_bar_create(row);
    lv_obj_set_size(bar, lv_pct(100), 9);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1e2a36), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    return bar;
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, SCREEN_W - UI_MARGIN * 2, HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, HEADER_Y);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_box = lv_obj_create(header);
    lv_obj_remove_style_all(title_box);
    lv_obj_set_size(title_box, 520, HEADER_H);
    lv_obj_set_flex_flow(title_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(title_box, 3, 0);

    lv_obj_t *title = create_label(title_box, "ESP32-S31 LVGL 9.5", &lv_font_montserrat_28, lv_color_hex(0xf3f7fb));
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_t *sub = create_label(title_box, "RGB565 media / PSRAM draw buffers / GT911 touch",
                                 &lv_font_montserrat_14, lv_color_hex(0x88a1b6));
    lv_obj_set_width(sub, lv_pct(100));

    lv_obj_t *mode_box = lv_obj_create(header);
    set_panel_style(mode_box, lv_color_hex(0x17212b), lv_color_hex(0x314255));
    lv_obj_set_size(mode_box, 220, 56);
    lv_obj_set_flex_flow(mode_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(mode_box, 4, 0);
    s_mode_label = create_label(mode_box, "Turbo mode: ON", &lv_font_montserrat_16, lv_color_hex(0x75f0d2));
    lv_obj_set_width(s_mode_label, lv_pct(100));
    s_status_label = create_label(mode_box,
                                  CONFIG_S31_LVGL_DEMO_ENABLE_PPA ? "PPA: enabled" : "PPA: disabled",
                                  &lv_font_montserrat_14, lv_color_hex(0xa9b8c7));
    lv_obj_set_width(s_status_label, lv_pct(100));
}

static void create_media_panel(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    set_panel_style(panel, lv_color_hex(0x101923), lv_color_hex(0x2d4357));
    lv_obj_set_size(panel, MEDIA_W, PANEL_H);
    lv_obj_set_pos(panel, UI_MARGIN, PANEL_Y);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 6, 0);

    create_panel_title(panel, "Media pipeline");

    lv_obj_t *image_row = lv_obj_create(panel);
    lv_obj_remove_style_all(image_row);
    lv_obj_set_size(image_row, lv_pct(100), 104);
    lv_obj_set_flex_flow(image_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(image_row, 10, 0);

    lv_obj_t *photo_slot = lv_obj_create(image_row);
    set_panel_style(photo_slot, lv_color_hex(0x0a141f), lv_color_hex(0x263d52));
    lv_obj_set_size(photo_slot, 158, 104);
    s_photo_image = lv_image_create(photo_slot);
    lv_image_set_src(s_photo_image, app_media_photo());
    lv_obj_set_size(s_photo_image, APP_MEDIA_PHOTO_W, APP_MEDIA_PHOTO_H);
    lv_image_set_pivot(s_photo_image, APP_MEDIA_PHOTO_W / 2, APP_MEDIA_PHOTO_H / 2);
    lv_image_set_antialias(s_photo_image, true);
    lv_obj_center(s_photo_image);

    lv_obj_t *anim_slot = lv_obj_create(image_row);
    set_panel_style(anim_slot, lv_color_hex(0x0a141f), lv_color_hex(0x263d52));
    lv_obj_set_size(anim_slot, 92, 104);

    lv_obj_t *anim = lv_animimg_create(anim_slot);
    static const void *anim_src[APP_MEDIA_ANIM_FRAME_COUNT];
    for (int i = 0; i < APP_MEDIA_ANIM_FRAME_COUNT; i++) {
        anim_src[i] = app_media_anim_frame(i);
    }
    lv_animimg_set_src(anim, anim_src, APP_MEDIA_ANIM_FRAME_COUNT);
    lv_animimg_set_duration(anim, 720);
    lv_animimg_set_repeat_count(anim, LV_ANIM_REPEAT_INFINITE);
    lv_obj_center(anim);
    lv_animimg_start(anim);

    s_video_canvas = lv_canvas_create(panel);
    lv_canvas_set_buffer(s_video_canvas, app_media_video_buffer(), APP_MEDIA_VIDEO_W, APP_MEDIA_VIDEO_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(s_video_canvas, APP_MEDIA_VIDEO_W, APP_MEDIA_VIDEO_H);
    lv_obj_set_style_radius(s_video_canvas, 8, 0);
    lv_obj_set_style_clip_corner(s_video_canvas, true, 0);

    s_fps_label = create_label(panel, "video -- fps / ui -- Hz", &lv_font_montserrat_14, lv_color_hex(0x88a1b6));
    lv_obj_set_width(s_fps_label, lv_pct(100));
}

static void create_control_panel(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    set_panel_style(panel, lv_color_hex(0x121c25), lv_color_hex(0x2b455e));
    lv_obj_set_size(panel, PERF_W, PANEL_H);
    lv_obj_set_pos(panel, UI_MARGIN + MEDIA_W + UI_GAP, PANEL_Y);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 10, 0);

    create_panel_title(panel, "Performance");
    s_cpu_bar = create_stat_bar(panel, "LVGL load", lv_color_hex(0x55d6ff));
    s_psram_bar = create_stat_bar(panel, "PSRAM use", lv_color_hex(0xf8ce62));
    s_bandwidth_bar = create_stat_bar(panel, "Video MB/s", lv_color_hex(0xff7c8a));

    lv_obj_t *arc_row = lv_obj_create(panel);
    lv_obj_remove_style_all(arc_row);
    lv_obj_set_size(arc_row, lv_pct(100), 96);
    lv_obj_set_flex_flow(arc_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(arc_row, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_temp_arc = lv_arc_create(arc_row);
    lv_obj_set_size(s_temp_arc, 82, 82);
    lv_arc_set_range(s_temp_arc, 0, 100);
    lv_arc_set_bg_angles(s_temp_arc, 135, 45);
    lv_obj_remove_style(s_temp_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_temp_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_temp_arc, lv_color_hex(0x213343), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_temp_arc, lv_color_hex(0x75f0d2), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_temp_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_temp_arc, 9, LV_PART_INDICATOR);

    s_meter_arc = lv_arc_create(arc_row);
    lv_obj_set_size(s_meter_arc, 82, 82);
    lv_arc_set_range(s_meter_arc, 0, 100);
    lv_arc_set_bg_angles(s_meter_arc, 135, 45);
    lv_obj_remove_style(s_meter_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_meter_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_meter_arc, lv_color_hex(0x213343), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_meter_arc, lv_color_hex(0xc58bff), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_meter_arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_meter_arc, 9, LV_PART_INDICATOR);

    s_heap_label = create_label(panel, "heap --K / psram --K", &lv_font_montserrat_14, lv_color_hex(0xa9b8c7));
    lv_obj_set_width(s_heap_label, lv_pct(100));
}

static void create_chart_panel(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    set_panel_style(panel, lv_color_hex(0x101821), lv_color_hex(0x2c4256));
    lv_obj_set_size(panel, CHART_W, PANEL_H);
    lv_obj_set_pos(panel, UI_MARGIN + MEDIA_W + UI_GAP + PERF_W + UI_GAP, PANEL_Y);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 8, 0);

    create_panel_title(panel, "Frame pacing");

    s_chart = lv_chart_create(panel);
    lv_obj_set_size(s_chart, lv_pct(100), 204);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINTS);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x0d141c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x263a4a), LV_PART_MAIN);
    s_chart_series = lv_chart_add_series(s_chart, lv_color_hex(0x75f0d2), LV_CHART_AXIS_PRIMARY_Y);
    for (int i = 0; i < CHART_POINTS; i++) {
        lv_chart_set_next_value(s_chart, s_chart_series, 30);
    }

    lv_obj_t *slider = lv_slider_create(panel);
    lv_obj_set_size(slider, lv_pct(100), 18);
    lv_slider_set_range(slider, 15, 60);
    lv_slider_set_value(slider, CONFIG_S31_LVGL_DEMO_VIDEO_FPS, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x203243), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x55d6ff), LV_PART_INDICATOR);

    lv_obj_t *hint = create_label(panel, "Touch slider to tune target load",
                                  &lv_font_montserrat_14, lv_color_hex(0x8da2b7));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
}

static void toggle_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    s_turbo_mode = !s_turbo_mode;
    lv_label_set_text(s_mode_label, s_turbo_mode ? "Turbo mode: ON" : "Turbo mode: ECO");
}

static void create_action_strip(lv_obj_t *screen)
{
    lv_obj_t *strip = lv_obj_create(screen);
    lv_obj_remove_style_all(strip);
    lv_obj_set_size(strip, SCREEN_W - UI_MARGIN * 2, STRIP_H);
    lv_obj_align(strip, LV_ALIGN_TOP_MID, 0, STRIP_Y);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *items[] = {"Picture", "Anim", "Video", "Touch", "PSRAM", "PPA"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *chip = lv_button_create(strip);
        lv_obj_set_size(chip, 118, 34);
        lv_obj_set_style_radius(chip, 7, 0);
        lv_obj_set_style_bg_color(chip, i == 5 ? lv_color_hex(0x21624f) : lv_color_hex(0x1a2b39), 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(0x2c6f8d), LV_STATE_PRESSED);
        lv_obj_add_event_cb(chip, toggle_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = create_label(chip, items[i], &lv_font_montserrat_14, lv_color_hex(0xf2f7fb));
        lv_obj_set_width(label, 104);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
    }
}

static void update_ui_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    static uint32_t phase;
    static int64_t last_report_us;
    static uint32_t last_video_frames;
    static uint32_t last_ui_ticks;

    phase++;
    s_ui_ticks++;

    if (s_video_dirty && s_video_canvas) {
        s_video_dirty = false;
        lv_obj_invalidate(s_video_canvas);
    }

    uint32_t load = (phase * 3 + (s_turbo_mode ? 30 : 12)) % 100;
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_used = psram_total ? (100 - psram_free * 100 / psram_total) : 0;
    uint32_t bw = (CONFIG_S31_LVGL_DEMO_VIDEO_FPS * APP_MEDIA_VIDEO_W * APP_MEDIA_VIDEO_H *
                   APP_MEDIA_RGB565_BYTES_PER_PIXEL) / (1024 * 1024);

    lv_bar_set_value(s_cpu_bar, load, LV_ANIM_OFF);
    lv_bar_set_value(s_psram_bar, psram_used, LV_ANIM_OFF);
    lv_bar_set_value(s_bandwidth_bar, bw * 6 > 100 ? 100 : (int32_t)(bw * 6), LV_ANIM_OFF);
    lv_arc_set_value(s_temp_arc, (int32_t)(35 + (phase % 45)));
    lv_arc_set_value(s_meter_arc, (int32_t)((phase * 5) % 100));
    lv_chart_set_next_value(s_chart, s_chart_series, (int32_t)(20 + ((phase * 7) % 72)));
    lv_image_set_rotation(s_photo_image, (int32_t)(((phase % 17) - 8) * 10));
    lv_image_set_scale(s_photo_image, 238 + ((phase % 18) * 2));

    int64_t now = esp_timer_get_time();
    if (last_report_us == 0) {
        last_report_us = now;
    }
    if (now - last_report_us >= 1000000) {
        uint32_t video_delta = s_video_frames - last_video_frames;
        uint32_t ui_delta = s_ui_ticks - last_ui_ticks;
        last_video_frames = s_video_frames;
        last_ui_ticks = s_ui_ticks;
        last_report_us = now;

        lv_label_set_text_fmt(s_fps_label, "video %" PRIu32 " fps / ui %" PRIu32 " Hz", video_delta, ui_delta);
        lv_label_set_text_fmt(s_heap_label, "heap %" PRIu32 "K / psram %" PRIu32 "K",
                              (uint32_t)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                              (uint32_t)(psram_free / 1024));
        ESP_LOGI(TAG, "video=%" PRIu32 "fps ui=%" PRIu32 "Hz psram_free=%" PRIu32 "KB",
                 video_delta, ui_delta, psram_free / 1024);
    }
}

void app_ui_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_size(screen, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x071018), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x122436), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    create_header(screen);
    create_action_strip(screen);
    create_media_panel(screen);
    create_control_panel(screen);
    create_chart_panel(screen);
}

void app_ui_start(void)
{
    lv_timer_create(update_ui_timer_cb, UI_UPDATE_PERIOD_MS, NULL);
}

void app_ui_notify_video_frame(void)
{
    s_video_frames++;
    s_video_dirty = true;
}

bool app_ui_turbo_enabled(void)
{
    return s_turbo_mode;
}
