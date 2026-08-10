/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

#define S31_TEST_TAG "s31_test"

typedef enum {
    TEST_PENDING,
    TEST_RUNNING,
    TEST_PASS,
    TEST_FAIL,
    TEST_MANUAL,
    TEST_SKIP,
} test_state_t;

typedef enum {
    ITEM_WIFI,
    ITEM_RTC,
    ITEM_IMU,
    ITEM_BQ27220,
    ITEM_TF_CARD,
    ITEM_RS485,
    ITEM_CANFD,
    ITEM_LCD,
    ITEM_TOUCH,
    ITEM_MIC,
    ITEM_SPEAKER,
    ITEM_USB_HOST,
    ITEM_COUNT,
} test_item_id_t;

typedef struct {
    const char *name;
    test_state_t state;
    char detail[96];
} test_item_t;

extern test_item_t s_items[ITEM_COUNT];
extern uint32_t s_touch_count;
extern volatile int32_t s_mic_level_l;
extern volatile int32_t s_mic_level_r;

const char *state_text(test_state_t state);
void set_item_result(test_item_id_t id, test_state_t state, const char *fmt, ...);

esp_err_t ui_init(void);
bool ui_touch_ready(void);
void ui_refresh_item(test_item_id_t id);
void ui_set_log(const char *fmt, ...);
void ui_update_audio(void);

esp_err_t test_lcd(void);
esp_err_t test_touch_check_ready(void);
void test_touch_record_press(int32_t x, int32_t y);

esp_err_t test_wifi_scan(void);
esp_err_t test_rtc(void);
esp_err_t test_imu(void);
esp_err_t test_bq27220(void);
esp_err_t test_tf_card(void);
esp_err_t test_rs485_canfd_bridge(void);

void test_speaker_play_tone(void);
void audio_task(void *arg);

esp_err_t test_usb_host_start(void);
void run_usb_host_only_test(void);
