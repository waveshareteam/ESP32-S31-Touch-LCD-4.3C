/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

#include "esp_board_manager_includes.h"
#include "esp_lv_adapter.h"
#include "s31_test.h"

#define TOUCH_MAX_POINTS  5
#define TOUCH_DOT_HISTORY 24
#define HEADER_HEIGHT     66

typedef struct {
    lv_obj_t *card;
    lv_obj_t *stripe;
    lv_obj_t *name;
    lv_obj_t *state;
    lv_obj_t *detail;
} ui_item_view_t;

static bool s_ui_ready;
static ui_item_view_t s_ui_items[ITEM_COUNT];
static lv_obj_t *s_summary_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_progress_bar;
static lv_obj_t *s_log_label;
static lv_obj_t *s_mic_label;
static lv_obj_t *s_mic_bar_l;
static lv_obj_t *s_mic_bar_r;
static lv_obj_t *s_touch_count_label;
static lv_obj_t *s_touch_layer;
static lv_obj_t *s_touch_dots[TOUCH_DOT_HISTORY];
static uint8_t s_touch_dot_index;
static uint16_t s_lcd_width;
static uint16_t s_lcd_height;
static lv_indev_t *s_touch_indev;

static lv_color_t ui_state_color(test_state_t state)
{
    switch (state) {
    case TEST_PASS:
        return lv_color_hex(0x36d399);
    case TEST_FAIL:
        return lv_color_hex(0xf87171);
    case TEST_RUNNING:
        return lv_color_hex(0xfbbf24);
    case TEST_MANUAL:
        return lv_color_hex(0x38bdf8);
    case TEST_SKIP:
        return lv_color_hex(0x94a3b8);
    case TEST_PENDING:
    default:
        return lv_color_hex(0x64748b);
    }
}

static void ui_count_states(int *pass, int *fail, int *manual, int *skip, int *running, int *pending)
{
    *pass = 0;
    *fail = 0;
    *manual = 0;
    *skip = 0;
    *running = 0;
    *pending = 0;

    for (int i = 0; i < ITEM_COUNT; i++) {
        switch (s_items[i].state) {
        case TEST_PASS:
            (*pass)++;
            break;
        case TEST_FAIL:
            (*fail)++;
            break;
        case TEST_MANUAL:
            (*manual)++;
            break;
        case TEST_SKIP:
            (*skip)++;
            break;
        case TEST_RUNNING:
            (*running)++;
            break;
        case TEST_PENDING:
        default:
            (*pending)++;
            break;
        }
    }
}

static void ui_refresh_summary_locked(void)
{
    if (!s_ui_ready) {
        return;
    }

    int pass = 0;
    int fail = 0;
    int manual = 0;
    int skip = 0;
    int running = 0;
    int pending = 0;
    ui_count_states(&pass, &fail, &manual, &skip, &running, &pending);

    int complete = pass + fail + manual + skip;
    lv_bar_set_value(s_progress_bar, complete * 100 / ITEM_COUNT, LV_ANIM_ON);
    lv_label_set_text_fmt(s_summary_label, "PASS %d   FAIL %d   MANUAL %d   SKIP %d",
                          pass, fail, manual, skip);

    if (fail > 0) {
        lv_label_set_text(s_status_label, "FAILURES DETECTED");
        lv_obj_set_style_text_color(s_status_label, ui_state_color(TEST_FAIL), 0);
    } else if (running > 0 || pending > 0) {
        lv_label_set_text(s_status_label, "AUTO TEST RUNNING");
        lv_obj_set_style_text_color(s_status_label, ui_state_color(TEST_RUNNING), 0);
    } else {
        lv_label_set_text(s_status_label, "AUTO TEST COMPLETE");
        lv_obj_set_style_text_color(s_status_label, ui_state_color(TEST_PASS), 0);
    }
}

static void ui_refresh_item_locked(test_item_id_t id)
{
    if (!s_ui_ready || id >= ITEM_COUNT || !s_ui_items[id].card) {
        return;
    }

    ui_item_view_t *view = &s_ui_items[id];
    lv_color_t color = ui_state_color(s_items[id].state);
    lv_obj_set_style_bg_color(view->stripe, color, 0);
    lv_obj_set_style_border_color(view->card, color, 0);
    lv_obj_set_style_text_color(view->state, color, 0);
    lv_label_set_text(view->name, s_items[id].name);
    lv_label_set_text(view->state, state_text(s_items[id].state));
    lv_label_set_text(view->detail, s_items[id].detail[0] ? s_items[id].detail : "waiting");
}

static void ui_refresh_all_locked(void)
{
    if (!s_ui_ready) {
        return;
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        ui_refresh_item_locked((test_item_id_t)i);
    }
    ui_refresh_summary_locked();
}

void ui_refresh_item(test_item_id_t id)
{
    if (!s_ui_ready) {
        return;
    }

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_refresh_item_locked(id);
        ui_refresh_summary_locked();
        esp_lv_adapter_unlock();
    }
}

static void ui_update_audio_locked(void)
{
    if (!s_ui_ready) {
        return;
    }

    int32_t left = s_mic_level_l;
    int32_t right = s_mic_level_r;
    if (left < 0) {
        left = 0;
    }
    if (right < 0) {
        right = 0;
    }
    if (left > 32767) {
        left = 32767;
    }
    if (right > 32767) {
        right = 32767;
    }

    lv_bar_set_value(s_mic_bar_l, left, LV_ANIM_OFF);
    lv_bar_set_value(s_mic_bar_r, right, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_mic_label, "MIC  L:%ld  R:%ld", (long)left, (long)right);
}

void ui_update_audio(void)
{
    if (!s_ui_ready) {
        return;
    }

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        ui_update_audio_locked();
        esp_lv_adapter_unlock();
    }
}

void ui_set_log(const char *fmt, ...)
{
    if (!s_ui_ready || !fmt) {
        return;
    }

    char line[128] = {0};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_label_set_text(s_log_label, line);
        esp_lv_adapter_unlock();
    }
}

static lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, lv_color_t color,
                                 int x, int y, int w)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(label, text);
    return label;
}

static void ui_add_touch_dot_locked(int32_t x, int32_t y)
{
    if (!s_ui_ready || !s_touch_layer) {
        return;
    }

    static const uint32_t colors[TOUCH_MAX_POINTS] = {
        0x38bdf8, 0x36d399, 0xfbbf24, 0xf87171, 0xc084fc,
    };
    lv_obj_t *dot = s_touch_dots[s_touch_dot_index];
    s_touch_dot_index = (s_touch_dot_index + 1) % TOUCH_DOT_HISTORY;

    if (!dot) {
        return;
    }

    if (x < 6) {
        x = 6;
    }
    if (y < 6) {
        y = 6;
    }
    if (x > s_lcd_width - 6) {
        x = s_lcd_width - 6;
    }
    int32_t local_y = y - HEADER_HEIGHT;
    if (local_y < 6) {
        local_y = 6;
    }
    if (local_y > s_lcd_height - HEADER_HEIGHT - 6) {
        local_y = s_lcd_height - HEADER_HEIGHT - 6;
    }

    lv_obj_set_pos(dot, x - 6, local_y - 6);
    lv_obj_set_style_bg_color(dot, lv_color_hex(colors[s_touch_dot_index % TOUCH_MAX_POINTS]), 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(dot, lv_obj_get_child_count(s_touch_layer) - 1);
}

static void ui_touch_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) {
        return;
    }

    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }

    lv_point_t point = {0};
    lv_indev_get_point(indev, &point);
    ui_add_touch_dot_locked(point.x, point.y);

    if (code == LV_EVENT_PRESSED) {
        test_touch_record_press(point.x, point.y);
        ui_refresh_item_locked(ITEM_TOUCH);
        ui_refresh_summary_locked();
        lv_label_set_text_fmt(s_touch_count_label, "TOUCH  count:%" PRIu32, s_touch_count);
        lv_label_set_text_fmt(s_log_label, "TOUCH  x:%ld  y:%ld",
                              (long)point.x, (long)point.y);
    }
}

static void ui_create_item_card(lv_obj_t *screen, test_item_id_t id, int x, int y)
{
    ui_item_view_t *view = &s_ui_items[id];
    view->card = lv_obj_create(screen);
    lv_obj_set_pos(view->card, x, y);
    lv_obj_set_size(view->card, 244, 68);
    lv_obj_remove_flag(view->card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(view->card, 6, 0);
    lv_obj_set_style_bg_color(view->card, lv_color_hex(0x1b222b), 0);
    lv_obj_set_style_border_width(view->card, 1, 0);
    lv_obj_set_style_border_color(view->card, ui_state_color(TEST_PENDING), 0);
    lv_obj_set_style_pad_all(view->card, 0, 0);

    view->stripe = lv_obj_create(view->card);
    lv_obj_set_pos(view->stripe, 0, 0);
    lv_obj_set_size(view->stripe, 7, 68);
    lv_obj_remove_flag(view->stripe, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(view->stripe, 0, 0);
    lv_obj_set_style_border_width(view->stripe, 0, 0);
    lv_obj_set_style_bg_color(view->stripe, ui_state_color(TEST_PENDING), 0);

    view->name = ui_create_label(view->card, s_items[id].name, lv_color_hex(0xf8fafc), 18, 8, 120);
    view->state = ui_create_label(view->card, state_text(s_items[id].state),
                                  ui_state_color(s_items[id].state), 160, 8, 70);
    lv_obj_set_style_text_align(view->state, LV_TEXT_ALIGN_RIGHT, 0);
    view->detail = ui_create_label(view->card, "waiting", lv_color_hex(0x94a3b8), 18, 36, 210);
}

static void ui_build_locked(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x10151d), 0);
    lv_obj_set_style_text_font(screen, LV_FONT_DEFAULT, 0);

    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, s_lcd_width, HEADER_HEIGHT);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x17202a), 0);

    lv_obj_t *title = ui_create_label(header, "ESP32-S31 COMPREHENSIVE TEST",
                                      lv_color_hex(0xf8fafc), 18, 4, 360);
    lv_obj_set_style_text_letter_space(title, 0, 0);

    lv_obj_t *subtitle = ui_create_label(header, "AUTO + MANUAL BOARD CHECK",
                                         lv_color_hex(0x94a3b8), 18, 27, 300);
    lv_obj_set_style_text_letter_space(subtitle, 0, 0);

    s_summary_label = ui_create_label(header, "PASS 0   FAIL 0   MANUAL 0   SKIP 0",
                                      lv_color_hex(0xe2e8f0), 420, 4, 360);
    lv_obj_set_style_text_align(s_summary_label, LV_TEXT_ALIGN_RIGHT, 0);

    s_status_label = ui_create_label(header, "AUTO TEST PENDING",
                                     ui_state_color(TEST_PENDING), 520, 27, 260);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_RIGHT, 0);

    s_progress_bar = lv_bar_create(header);
    lv_obj_set_pos(s_progress_bar, 18, 53);
    lv_obj_set_size(s_progress_bar, 762, 6);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x263241), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

    const int start_x = 18;
    const int start_y = HEADER_HEIGHT + 12;
    const int card_w = 244;
    const int card_h = 68;
    const int gap_x = 16;
    const int gap_y = 8;
    for (int i = 0; i < ITEM_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        ui_create_item_card(screen, (test_item_id_t)i,
                            start_x + col * (card_w + gap_x),
                            start_y + row * (card_h + gap_y));
    }

    lv_obj_t *footer = lv_obj_create(screen);
    lv_obj_set_pos(footer, 0, 390);
    lv_obj_set_size(footer, s_lcd_width, 90);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(footer, 0, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x151b24), 0);

    s_mic_label = ui_create_label(footer, "MIC  L:0  R:0", lv_color_hex(0xe2e8f0), 18, 12, 220);
    ui_create_label(footer, "L", lv_color_hex(0x94a3b8), 18, 39, 14);
    ui_create_label(footer, "R", lv_color_hex(0x94a3b8), 18, 61, 14);

    s_mic_bar_l = lv_bar_create(footer);
    lv_obj_set_pos(s_mic_bar_l, 40, 42);
    lv_obj_set_size(s_mic_bar_l, 280, 10);
    lv_bar_set_range(s_mic_bar_l, 0, 32767);
    lv_obj_set_style_bg_color(s_mic_bar_l, lv_color_hex(0x263241), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_mic_bar_l, lv_color_hex(0x36d399), LV_PART_INDICATOR);

    s_mic_bar_r = lv_bar_create(footer);
    lv_obj_set_pos(s_mic_bar_r, 40, 64);
    lv_obj_set_size(s_mic_bar_r, 280, 10);
    lv_bar_set_range(s_mic_bar_r, 0, 32767);
    lv_obj_set_style_bg_color(s_mic_bar_r, lv_color_hex(0x263241), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_mic_bar_r, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

    s_touch_count_label = ui_create_label(footer, "TOUCH  count:0",
                                          lv_color_hex(0xe2e8f0), 354, 12, 190);
    ui_create_label(footer, "Tap anywhere to verify GT911",
                    lv_color_hex(0x94a3b8), 354, 39, 220);

    s_log_label = ui_create_label(footer, "Waiting for test results",
                                  lv_color_hex(0x94a3b8), 18, 76, 762);

    s_touch_layer = lv_obj_create(screen);
    lv_obj_set_pos(s_touch_layer, 0, HEADER_HEIGHT);
    lv_obj_set_size(s_touch_layer, s_lcd_width, s_lcd_height - HEADER_HEIGHT);
    lv_obj_add_flag(s_touch_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_touch_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_touch_layer, 0, 0);
    lv_obj_set_style_border_width(s_touch_layer, 0, 0);
    lv_obj_set_style_bg_opa(s_touch_layer, LV_OPA_0, 0);
    lv_obj_add_event_cb(s_touch_layer, ui_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_touch_layer, ui_touch_event_cb, LV_EVENT_PRESSING, NULL);

    for (int i = 0; i < TOUCH_DOT_HISTORY; i++) {
        s_touch_dots[i] = lv_obj_create(s_touch_layer);
        lv_obj_set_size(s_touch_dots[i], 12, 12);
        lv_obj_add_flag(s_touch_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_touch_dots[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(s_touch_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_touch_dots[i], 2, 0);
        lv_obj_set_style_border_color(s_touch_dots[i], lv_color_hex(0xffffff), 0);
        lv_obj_set_style_border_opa(s_touch_dots[i], LV_OPA_60, 0);
        lv_obj_set_style_bg_color(s_touch_dots[i], lv_color_hex(0x38bdf8), 0);
        lv_obj_set_style_bg_opa(s_touch_dots[i], LV_OPA_80, 0);
    }
}

bool ui_touch_ready(void)
{
    return s_touch_indev != NULL;
}

esp_err_t ui_init(void)
{
    if (s_ui_ready) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    bool display_initialized = false;
    bool touch_initialized = false;
    bool brightness_initialized = false;
    bool adapter_initialized = false;
    bool adapter_locked = false;
    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *lcd_config = NULL;
    dev_lcd_touch_handles_t *touch = NULL;

    ret = esp_board_manager_init_device_by_name("display_lcd");
    if (ret != ESP_OK) {
        goto cleanup;
    }
    display_initialized = true;
    ret = esp_board_manager_init_device_by_name("lcd_touch");
    if (ret != ESP_OK) {
        goto cleanup;
    }
    touch_initialized = true;
    ret = esp_board_manager_init_device_by_name("lcd_brightness");
    if (ret != ESP_OK) {
        goto cleanup;
    }
    brightness_initialized = true;

    ret = esp_board_manager_get_device_handle("display_lcd", (void **)&lcd);
    if (ret != ESP_OK || !lcd || !lcd->panel_handle) {
        ret = ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
        goto cleanup;
    }
    ret = esp_board_manager_get_device_config("display_lcd", (void **)&lcd_config);
    if (ret != ESP_OK || !lcd_config || lcd_config->lcd_width == 0 || lcd_config->lcd_height == 0) {
        ret = ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
        goto cleanup;
    }
    ret = esp_board_manager_get_device_handle("lcd_touch", (void **)&touch);
    if (ret != ESP_OK || !touch || !touch->touch_handle) {
        ret = ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    s_lcd_width = lcd_config->lcd_width;
    s_lcd_height = lcd_config->lcd_height;
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_priority = 4;
    adapter_config.task_stack_size = 8192;
    adapter_config.tick_period_ms = 5;
    ret = esp_lv_adapter_init(&adapter_config);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    adapter_initialized = true;

    esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(lcd->panel_handle, lcd->io_handle,
                                                   s_lcd_width, s_lcd_height,
                                                   ESP_LV_ADAPTER_ROTATE_0);
    display_config.profile.buffer_height = 80;
    display_config.profile.use_psram = true;
    display_config.profile.require_double_buffer = true;
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT;
    lv_display_t *display = esp_lv_adapter_register_display(&display_config);
    if (!display) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    const esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch->touch_handle);
    s_touch_indev = esp_lv_adapter_register_touch(&touch_config);
    if (!s_touch_indev) {
        ret = ESP_FAIL;
        goto cleanup;
    }
    ret = esp_lv_adapter_start();
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = esp_lv_adapter_lock(-1);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    adapter_locked = true;
    ui_build_locked();
    s_ui_ready = true;
    ui_refresh_all_locked();
    ui_update_audio_locked();
    esp_lv_adapter_unlock();
    return ESP_OK;

cleanup:
    if (adapter_locked) {
        esp_lv_adapter_unlock();
    }
    s_touch_indev = NULL;
    if (adapter_initialized) {
        esp_lv_adapter_deinit();
    }
    if (brightness_initialized) {
        esp_board_manager_deinit_device_by_name("lcd_brightness");
    }
    if (touch_initialized) {
        esp_board_manager_deinit_device_by_name("lcd_touch");
    }
    if (display_initialized) {
        esp_board_manager_deinit_device_by_name("display_lcd");
    }
    ESP_LOGE(S31_TEST_TAG, "LVGL Board Manager initialization failed: %s", esp_err_to_name(ret));
    return ret;
}
