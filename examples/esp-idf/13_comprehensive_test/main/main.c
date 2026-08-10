/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "s31_test.h"

test_item_t s_items[ITEM_COUNT] = {
    [ITEM_WIFI] = {.name = "WIFI"},
    [ITEM_RTC] = {.name = "RTC"},
    [ITEM_IMU] = {.name = "IMU"},
    [ITEM_BQ27220] = {.name = "BQ27220"},
    [ITEM_TF_CARD] = {.name = "TF CARD"},
    [ITEM_RS485] = {.name = "RS485"},
    [ITEM_CANFD] = {.name = "CANFD"},
    [ITEM_LCD] = {.name = "LCD"},
    [ITEM_TOUCH] = {.name = "TOUCH"},
    [ITEM_MIC] = {.name = "MIC"},
    [ITEM_SPEAKER] = {.name = "SPEAKER"},
    [ITEM_USB_HOST] = {.name = "USB HOST"},
};

uint32_t s_touch_count;
volatile int32_t s_mic_level_l;
volatile int32_t s_mic_level_r;

const char *state_text(test_state_t state)
{
    switch (state) {
    case TEST_PASS:
        return "PASS";
    case TEST_FAIL:
        return "FAIL";
    case TEST_RUNNING:
        return "RUN";
    case TEST_MANUAL:
        return "MANUAL";
    case TEST_SKIP:
        return "SKIP";
    case TEST_PENDING:
    default:
        return "PENDING";
    }
}

void set_item_result(test_item_id_t id, test_state_t state, const char *fmt, ...)
{
    if (id >= ITEM_COUNT) {
        return;
    }

    s_items[id].state = state;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(s_items[id].detail, sizeof(s_items[id].detail), fmt, ap);
        va_end(ap);
    } else {
        s_items[id].detail[0] = '\0';
    }

    ESP_LOGI(S31_TEST_TAG, "%-8s %-7s %s",
             s_items[id].name, state_text(state), s_items[id].detail);
    ui_refresh_item(id);
    ui_set_log("%s  %s  %s", s_items[id].name, state_text(state), s_items[id].detail);
}

static void run_rs485_canfd_bridge_item(void)
{
    set_item_result(ITEM_CANFD, TEST_RUNNING, "64B bridge tx first");
    set_item_result(ITEM_RS485, TEST_RUNNING, "64B bridge rx first");
    esp_err_t ret = test_rs485_canfd_bridge();
    if (ret != ESP_OK) {
        if (s_items[ITEM_RS485].state == TEST_RUNNING) {
            set_item_result(ITEM_RS485, TEST_FAIL, "%s", esp_err_to_name(ret));
        }
        if (s_items[ITEM_CANFD].state == TEST_RUNNING) {
            set_item_result(ITEM_CANFD, TEST_FAIL, "%s", esp_err_to_name(ret));
        }
    }
}

static void run_test_item(test_item_id_t id, esp_err_t (*fn)(void))
{
    set_item_result(id, TEST_RUNNING, "running");
    esp_err_t ret = fn();
    if (ret != ESP_OK && s_items[id].state != TEST_SKIP) {
        set_item_result(id, TEST_FAIL, "%s", esp_err_to_name(ret));
    }
}

static void log_summary(void)
{
    int pass = 0;
    int fail = 0;
    int manual = 0;
    int skip = 0;
    for (int i = 0; i < ITEM_COUNT; i++) {
        switch (s_items[i].state) {
        case TEST_PASS:
            pass++;
            break;
        case TEST_FAIL:
            fail++;
            break;
        case TEST_MANUAL:
            manual++;
            break;
        case TEST_SKIP:
            skip++;
            break;
        default:
            break;
        }
    }

    ESP_LOGI(S31_TEST_TAG, "SUMMARY pass=%d fail=%d manual=%d skip=%d",
             pass, fail, manual, skip);
    for (int i = 0; i < ITEM_COUNT; i++) {
        ESP_LOGI(S31_TEST_TAG, "SUMMARY %-8s %-7s %s",
                 s_items[i].name, state_text(s_items[i].state), s_items[i].detail);
    }
}

static void run_full_test(void)
{
    test_lcd();
    test_touch_check_ready();
    set_item_result(ITEM_MIC, TEST_MANUAL, "audio task pending");
    set_item_result(ITEM_SPEAKER, TEST_MANUAL, "tone pending");
    run_test_item(ITEM_USB_HOST, test_usb_host_start);

    run_test_item(ITEM_WIFI, test_wifi_scan);
    run_test_item(ITEM_RTC, test_rtc);
    run_test_item(ITEM_IMU, test_imu);
    run_test_item(ITEM_BQ27220, test_bq27220);
    run_test_item(ITEM_TF_CARD, test_tf_card);
    run_rs485_canfd_bridge_item();

    BaseType_t task_ok = xTaskCreate(audio_task, "audio_test", 4096, NULL, 5, NULL);
    if (task_ok != pdPASS) {
        set_item_result(ITEM_MIC, TEST_FAIL, "audio task create failed");
        set_item_result(ITEM_SPEAKER, TEST_FAIL, "audio task create failed");
    }

    log_summary();

    int64_t last_redraw = 0;
    while (true) {
        int64_t now = esp_timer_get_time();
        if (now - last_redraw >= 100 * 1000) {
            ui_update_audio();
            last_redraw = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
#if CONFIG_S31_TEST_USB_HOST_ONLY
    run_usb_host_only_test();
#else
    run_full_test();
#endif
}
