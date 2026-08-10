/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "music_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "music_ui"

#define MUSIC_ATTR_TITLE        0x01
#define MUSIC_ATTR_ARTIST       0x02
#define MUSIC_ATTR_ALBUM        0x04
#define MUSIC_ATTR_GENRE        0x20
#define MUSIC_ATTR_PLAYING_TIME 0x40

LV_FONT_DECLARE(font_puhui_30_4);
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_puhui_16_4);

#define PLAYBACK_STOPPED  0
#define PLAYBACK_PLAYING  1
#define PLAYBACK_PAUSED   2
#define PLAYBACK_FWD_SEEK 3
#define PLAYBACK_REV_SEEK 4
#define PLAYBACK_UNKNOWN  0xFE
#define PLAYBACK_ERROR    0xFF

#define UI_TEXT_MAX 192

#define UI_FONT_TITLE (&font_puhui_30_4)
#define UI_FONT_BODY  (&font_puhui_20_4)
#define UI_FONT_SMALL (&font_puhui_16_4)

typedef struct {
    bool ready;
    bool connected;
    uint8_t playback_status;
    uint32_t position_ms;
    uint32_t duration_ms;
    int sample_rate;
    int channel_count;
    char stream_state[24];
    char title[UI_TEXT_MAX];
    char artist[UI_TEXT_MAX];
    char album[UI_TEXT_MAX];
    char genre[UI_TEXT_MAX];
    lv_obj_t *screen;
    lv_obj_t *status_chip;
    lv_obj_t *status_label;
    lv_obj_t *stream_label;
    lv_obj_t *codec_label;
    lv_obj_t *title_label;
    lv_obj_t *artist_label;
    lv_obj_t *album_label;
    lv_obj_t *genre_label;
    lv_obj_t *time_label;
    lv_obj_t *progress_bar;
} music_ui_t;

static music_ui_t s_ui;

static bool ui_utf8_is_cont(uint8_t ch)
{
    return (ch & 0xC0) == 0x80;
}

static size_t ui_utf8_char_len(const uint8_t *src, size_t remaining)
{
    if (!src || remaining == 0) {
        return 0;
    }

    uint8_t ch = src[0];
    if (ch < 0x80) {
        return 1;
    }
    if (ch >= 0xC2 && ch <= 0xDF) {
        return remaining >= 2 && ui_utf8_is_cont(src[1]) ? 2 : 0;
    }
    if (ch >= 0xE0 && ch <= 0xEF) {
        if (remaining < 3 || !ui_utf8_is_cont(src[1]) || !ui_utf8_is_cont(src[2])) {
            return 0;
        }
        if (ch == 0xE0 && src[1] < 0xA0) {
            return 0;
        }
        if (ch == 0xED && src[1] >= 0xA0) {
            return 0;
        }
        return 3;
    }
    if (ch >= 0xF0 && ch <= 0xF4) {
        if (remaining < 4 || !ui_utf8_is_cont(src[1]) || !ui_utf8_is_cont(src[2]) || !ui_utf8_is_cont(src[3])) {
            return 0;
        }
        if (ch == 0xF0 && src[1] < 0x90) {
            return 0;
        }
        if (ch == 0xF4 && src[1] > 0x8F) {
            return 0;
        }
        return 4;
    }
    return 0;
}

static void ui_copy_text(char *dst, size_t dst_size, const uint8_t *src, size_t len)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || len == 0) {
        dst[0] = '\0';
        return;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;
    while (in_pos < len && out_pos < dst_size - 1) {
        size_t char_len = ui_utf8_char_len(src + in_pos, len - in_pos);
        if (char_len == 0) {
            dst[out_pos++] = '?';
            in_pos++;
            continue;
        }
        if (out_pos + char_len >= dst_size) {
            break;
        }
        memcpy(dst + out_pos, src + in_pos, char_len);
        out_pos += char_len;
        in_pos += char_len;
    }
    dst[out_pos] = '\0';
}

static const char *ui_text_or_dash(const char *text)
{
    return (text && text[0]) ? text : "-";
}

static const lv_font_t *ui_font_for_text(const char *text, const lv_font_t *preferred)
{
    (void)text;
    return preferred ? preferred : UI_FONT_BODY;
}

static bool ui_label_set_text_if_changed(lv_obj_t *label, const char *text, const lv_font_t *font)
{
    if (!label || !text) {
        return false;
    }

    bool changed = false;
    const lv_font_t *active_font = font ? font : UI_FONT_BODY;
    if (lv_obj_get_style_text_font(label, LV_PART_MAIN) != active_font) {
        lv_obj_set_style_text_font(label, active_font, 0);
        changed = true;
    }

    const char *old_text = lv_label_get_text(label);
    if (!old_text || strcmp(old_text, text) != 0) {
        lv_label_set_text(label, text);
        changed = true;
    }
    return changed;
}

static bool ui_label_set_text_adaptive(lv_obj_t *label, const char *text, const lv_font_t *preferred)
{
    return ui_label_set_text_if_changed(label, text, ui_font_for_text(text, preferred));
}

static const char *ui_playback_text(uint8_t status)
{
    switch (status) {
    case PLAYBACK_STOPPED:
        return "Stopped";
    case PLAYBACK_PLAYING:
        return "Playing";
    case PLAYBACK_PAUSED:
        return "Paused";
    case PLAYBACK_FWD_SEEK:
        return "Seeking";
    case PLAYBACK_REV_SEEK:
        return "Seeking";
    case PLAYBACK_ERROR:
        return "Playback error";
    default:
        return "Connected";
    }
}

static lv_color_t ui_status_color(void)
{
    if (!s_ui.connected) {
        return lv_color_hex(0x4B5563);
    }
    if (s_ui.playback_status == PLAYBACK_PLAYING) {
        return lv_color_hex(0x14B8A6);
    }
    if (s_ui.playback_status == PLAYBACK_PAUSED) {
        return lv_color_hex(0xF59E0B);
    }
    return lv_color_hex(0x64748B);
}

static void ui_set_bg_color_if_changed(lv_obj_t *obj, lv_color_t color, lv_part_t part)
{
    if (!obj) {
        return;
    }
    if (!lv_color_eq(lv_obj_get_style_bg_color(obj, part), color)) {
        lv_obj_set_style_bg_color(obj, color, part);
    }
}

static void ui_format_time(uint32_t ms, char *buf, size_t buf_size)
{
    if (ms == 0) {
        snprintf(buf, buf_size, "--:--");
        return;
    }

    uint32_t total_sec = ms / 1000;
    snprintf(buf, buf_size, "%02" PRIu32 ":%02" PRIu32, total_sec / 60, total_sec % 60);
}

static lv_obj_t *ui_create_panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x243244), 0);
    lv_obj_set_style_pad_all(panel, 18, 0);
    return panel;
}

static lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text);
    return label;
}

static void ui_refresh_locked(void)
{
    if (!s_ui.ready) {
        return;
    }

    ui_set_bg_color_if_changed(s_ui.status_chip, ui_status_color(), LV_PART_MAIN);
    ui_label_set_text_if_changed(s_ui.status_label,
                                 s_ui.connected ? ui_playback_text(s_ui.playback_status) : "Waiting for phone",
                                 UI_FONT_SMALL);
    ui_label_set_text_if_changed(s_ui.stream_label, s_ui.stream_state[0] ? s_ui.stream_state : "A2DP: Idle",
                                 UI_FONT_BODY);

    char codec[48];
    if (s_ui.sample_rate > 0) {
        snprintf(codec, sizeof(codec), "%d Hz / %s", s_ui.sample_rate,
                 s_ui.channel_count == 1 ? "Mono" : "Stereo");
    } else {
        snprintf(codec, sizeof(codec), "Codec: -");
    }
    ui_label_set_text_if_changed(s_ui.codec_label, codec, UI_FONT_BODY);

    ui_label_set_text_adaptive(s_ui.title_label, s_ui.title[0] ? s_ui.title : "No track metadata", UI_FONT_TITLE);

    char line[UI_TEXT_MAX + 16];
    snprintf(line, sizeof(line), "Artist  %s", ui_text_or_dash(s_ui.artist));
    ui_label_set_text_adaptive(s_ui.artist_label, line, UI_FONT_BODY);
    snprintf(line, sizeof(line), "Album   %s", ui_text_or_dash(s_ui.album));
    ui_label_set_text_adaptive(s_ui.album_label, line, UI_FONT_SMALL);
    snprintf(line, sizeof(line), "Genre   %s", ui_text_or_dash(s_ui.genre));
    ui_label_set_text_adaptive(s_ui.genre_label, line, UI_FONT_SMALL);

    char pos[12];
    char dur[12];
    ui_format_time(s_ui.position_ms, pos, sizeof(pos));
    ui_format_time(s_ui.duration_ms, dur, sizeof(dur));
    snprintf(line, sizeof(line), "%s / %s", pos, dur);
    ui_label_set_text_if_changed(s_ui.time_label, line, UI_FONT_SMALL);

    int32_t progress = 0;
    if (s_ui.duration_ms > 0 && s_ui.position_ms <= s_ui.duration_ms) {
        progress = (int32_t)(((uint64_t)s_ui.position_ms * 1000) / s_ui.duration_ms);
    }
    if (lv_bar_get_value(s_ui.progress_bar) != progress) {
        lv_bar_set_value(s_ui.progress_bar, progress, LV_ANIM_OFF);
    }
}

static void ui_refresh(void)
{
    if (!s_ui.ready) {
        return;
    }
    if (esp_lv_adapter_lock(100) == ESP_OK) {
        ui_refresh_locked();
        esp_lv_adapter_unlock();
    }
}

static void ui_build_locked(void)
{
    s_ui.screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_ui.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_hex(0x0B1020), 0);
    lv_obj_set_style_bg_opa(s_ui.screen, LV_OPA_COVER, 0);

    lv_obj_t *header = ui_create_label(s_ui.screen, "ESP Speaker", UI_FONT_BODY, lv_color_hex(0xF8FAFC));
    lv_obj_set_pos(header, 28, 18);

    s_ui.status_chip = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.status_chip, 188, 36);
    lv_obj_set_pos(s_ui.status_chip, 584, 14);
    lv_obj_clear_flag(s_ui.status_chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_ui.status_chip, 18, 0);
    lv_obj_set_style_border_width(s_ui.status_chip, 0, 0);
    lv_obj_set_style_pad_all(s_ui.status_chip, 0, 0);
    s_ui.status_label = ui_create_label(s_ui.status_chip, "", UI_FONT_SMALL, lv_color_hex(0xFFFFFF));
    lv_obj_center(s_ui.status_label);

    lv_obj_t *main_panel = ui_create_panel(s_ui.screen, 28, 70, 516, 304);
    lv_obj_t *now = ui_create_label(main_panel, "NOW PLAYING", UI_FONT_SMALL, lv_color_hex(0x5EEAD4));
    lv_obj_set_pos(now, 0, 0);

    s_ui.title_label = ui_create_label(main_panel, "", UI_FONT_TITLE, lv_color_hex(0xF8FAFC));
    lv_obj_set_width(s_ui.title_label, 468);
    lv_obj_set_height(s_ui.title_label, 82);
    lv_obj_set_pos(s_ui.title_label, 0, 44);
    lv_label_set_long_mode(s_ui.title_label, LV_LABEL_LONG_DOT);

    s_ui.artist_label = ui_create_label(main_panel, "", UI_FONT_BODY, lv_color_hex(0xCBD5E1));
    lv_obj_set_width(s_ui.artist_label, 468);
    lv_obj_set_pos(s_ui.artist_label, 0, 152);
    lv_label_set_long_mode(s_ui.artist_label, LV_LABEL_LONG_DOT);

    s_ui.album_label = ui_create_label(main_panel, "", UI_FONT_SMALL, lv_color_hex(0x94A3B8));
    lv_obj_set_width(s_ui.album_label, 468);
    lv_obj_set_pos(s_ui.album_label, 0, 196);
    lv_label_set_long_mode(s_ui.album_label, LV_LABEL_LONG_DOT);

    s_ui.genre_label = ui_create_label(main_panel, "", UI_FONT_SMALL, lv_color_hex(0x94A3B8));
    lv_obj_set_width(s_ui.genre_label, 468);
    lv_obj_set_pos(s_ui.genre_label, 0, 232);
    lv_label_set_long_mode(s_ui.genre_label, LV_LABEL_LONG_DOT);

    lv_obj_t *side_panel = ui_create_panel(s_ui.screen, 566, 70, 206, 304);
    lv_obj_t *link_title = ui_create_label(side_panel, "LINK", UI_FONT_SMALL, lv_color_hex(0x93C5FD));
    lv_obj_set_pos(link_title, 0, 0);
    s_ui.stream_label = ui_create_label(side_panel, "", UI_FONT_BODY, lv_color_hex(0xE2E8F0));
    lv_obj_set_width(s_ui.stream_label, 160);
    lv_obj_set_pos(s_ui.stream_label, 0, 42);
    lv_label_set_long_mode(s_ui.stream_label, LV_LABEL_LONG_WRAP);

    lv_obj_t *codec_title = ui_create_label(side_panel, "AUDIO", UI_FONT_SMALL, lv_color_hex(0xFCD34D));
    lv_obj_set_pos(codec_title, 0, 134);
    s_ui.codec_label = ui_create_label(side_panel, "", UI_FONT_BODY, lv_color_hex(0xE2E8F0));
    lv_obj_set_width(s_ui.codec_label, 160);
    lv_obj_set_pos(s_ui.codec_label, 0, 176);
    lv_label_set_long_mode(s_ui.codec_label, LV_LABEL_LONG_WRAP);

    lv_obj_t *bottom_panel = ui_create_panel(s_ui.screen, 28, 396, 744, 58);
    s_ui.progress_bar = lv_bar_create(bottom_panel);
    lv_obj_set_size(s_ui.progress_bar, 538, 16);
    lv_obj_set_pos(s_ui.progress_bar, 0, 4);
    lv_bar_set_range(s_ui.progress_bar, 0, 1000);
    lv_obj_set_style_radius(s_ui.progress_bar, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.progress_bar, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(0x263244), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(0x14B8A6), LV_PART_INDICATOR);

    s_ui.time_label = ui_create_label(bottom_panel, "--:-- / --:--", UI_FONT_SMALL, lv_color_hex(0xE2E8F0));
    lv_obj_set_width(s_ui.time_label, 158);
    lv_obj_set_pos(s_ui.time_label, 560, 0);
    lv_label_set_long_mode(s_ui.time_label, LV_LABEL_LONG_DOT);

    lv_screen_load_anim(s_ui.screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

esp_err_t music_ui_start(void)
{
    if (s_ui.ready) {
        return ESP_OK;
    }

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.playback_status = PLAYBACK_UNKNOWN;
    snprintf(s_ui.stream_state, sizeof(s_ui.stream_state), "A2DP: Idle");

    void *lcd_handle = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    ESP_RETURN_ON_ERROR(
        esp_board_manager_init_device_by_name("display_lcd"),
        TAG, "initialize Board Manager LCD failed");
    ESP_RETURN_ON_ERROR(
        esp_board_manager_get_device_handle("display_lcd", &lcd_handle),
        TAG, "get Board Manager LCD handle failed");
    ESP_RETURN_ON_ERROR(
        esp_board_manager_get_device_config("display_lcd", (void **)&lcd_cfg),
        TAG, "get Board Manager LCD config failed");
    ESP_RETURN_ON_FALSE(lcd_handle && lcd_cfg, ESP_ERR_INVALID_STATE,
                        TAG, "Board Manager LCD data is incomplete");

    dev_display_lcd_handles_t *lcd = (dev_display_lcd_handles_t *)lcd_handle;
    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_priority = 4;
    adapter_cfg.task_stack_size = 6 * 1024;
    adapter_cfg.tick_period_ms = 5;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_cfg), TAG, "initialize LVGL adapter failed");

    esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
        lcd->panel_handle, lcd->io_handle, lcd_cfg->lcd_width, lcd_cfg->lcd_height,
        ESP_LV_ADAPTER_ROTATE_0);
    display_cfg.profile.buffer_height = 100;
    display_cfg.profile.use_psram = true;
    display_cfg.profile.require_double_buffer = true;
    display_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT;
    ESP_RETURN_ON_FALSE(esp_lv_adapter_register_display(&display_cfg), ESP_FAIL,
                        TAG, "register Board Manager LCD with LVGL failed");
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "start LVGL adapter failed");

    esp_err_t brightness_ret = esp_board_manager_init_device_by_name("lcd_brightness");
    if (brightness_ret != ESP_OK) {
        ESP_LOGW(TAG, "backlight initialization failed: %s", esp_err_to_name(brightness_ret));
    }

    ESP_RETURN_ON_ERROR(esp_lv_adapter_lock(-1), TAG, "display lock failed");
    ui_build_locked();
    s_ui.ready = true;
    ui_refresh_locked();
    esp_lv_adapter_unlock();
    return ESP_OK;
}

void music_ui_set_peer_connected(bool connected)
{
    if (s_ui.connected == connected) {
        return;
    }

    s_ui.connected = connected;

    if (!connected) {
        s_ui.playback_status = PLAYBACK_UNKNOWN;
        s_ui.position_ms = 0;
        s_ui.duration_ms = 0;
        s_ui.sample_rate = 0;
        s_ui.channel_count = 0;
        snprintf(s_ui.stream_state, sizeof(s_ui.stream_state), "A2DP: Idle");
    }

    ui_refresh();
}

void music_ui_set_stream_state(const char *state)
{
    snprintf(s_ui.stream_state, sizeof(s_ui.stream_state), "A2DP: %s", state ? state : "Idle");
    ui_refresh();
}

void music_ui_set_codec_info(int sample_rate, int channel_count)
{
    s_ui.sample_rate = sample_rate;
    s_ui.channel_count = channel_count;
    ui_refresh();
}

void music_ui_set_playback_status(uint8_t status)
{
    s_ui.playback_status = status;
    ui_refresh();
}

void music_ui_set_play_position(uint32_t position_ms)
{
    s_ui.position_ms = position_ms == UINT32_MAX ? 0 : position_ms;
    ui_refresh();
}

void music_ui_set_play_duration(uint32_t duration_ms)
{
    s_ui.duration_ms = duration_ms == UINT32_MAX ? 0 : duration_ms;
    ui_refresh();
}

void music_ui_set_metadata_attr(uint8_t attr_id, const uint8_t *text, size_t len)
{
    bool need_refresh = true;

    switch (attr_id) {
    case MUSIC_ATTR_TITLE:
        ui_copy_text(s_ui.title, sizeof(s_ui.title), text, len);
        if (s_ui.ready && s_ui.title_label) {
            if (esp_lv_adapter_lock(100) == ESP_OK) {
                ui_label_set_text_adaptive(
                    s_ui.title_label,
                    s_ui.title[0] ? s_ui.title : "",
                    UI_FONT_TITLE
                );
                esp_lv_adapter_unlock();
            }
        }
        return;

    case MUSIC_ATTR_ARTIST:
        ui_copy_text(s_ui.artist, sizeof(s_ui.artist), text, len);
        break;

    case MUSIC_ATTR_ALBUM:
        ui_copy_text(s_ui.album, sizeof(s_ui.album), text, len);
        break;

    case MUSIC_ATTR_GENRE:
        ui_copy_text(s_ui.genre, sizeof(s_ui.genre), text, len);
        break;

    case MUSIC_ATTR_PLAYING_TIME: {
        char value[16];
        ui_copy_text(value, sizeof(value), text, len);
        s_ui.duration_ms = (uint32_t)strtoul(value, NULL, 10);
        break;
    }

    default:
        need_refresh = false;
        break;
    }

    if (need_refresh) {
        ui_refresh();
    }
}

void music_ui_clear_metadata(void)
{
    s_ui.title[0] = '\0';
    s_ui.artist[0] = '\0';
    s_ui.album[0] = '\0';
    s_ui.genre[0] = '\0';
    s_ui.position_ms = 0;
    s_ui.duration_ms = 0;
    ui_refresh();
}
