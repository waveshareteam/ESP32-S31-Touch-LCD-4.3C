/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"

#include "s31_test.h"

esp_err_t test_lcd(void)
{
    if (ui_init() == ESP_OK) {
        set_item_result(ITEM_LCD, TEST_MANUAL, "check LVGL dashboard");
        return ESP_OK;
    }

    set_item_result(ITEM_LCD, TEST_FAIL, "display init failed");
    return ESP_FAIL;
}

esp_err_t test_touch_check_ready(void)
{
    if (!ui_touch_ready()) {
        set_item_result(ITEM_TOUCH, TEST_FAIL, "touch init failed");
        return ESP_FAIL;
    }

    set_item_result(ITEM_TOUCH, TEST_MANUAL, "tap screen");
    return ESP_OK;
}

void test_touch_record_press(int32_t x, int32_t y)
{
    s_touch_count++;
    s_items[ITEM_TOUCH].state = TEST_PASS;
    snprintf(s_items[ITEM_TOUCH].detail, sizeof(s_items[ITEM_TOUCH].detail),
             "touch count=%" PRIu32, s_touch_count);
    ESP_LOGI(S31_TEST_TAG, "%-8s %-7s %s x=%ld y=%ld",
             s_items[ITEM_TOUCH].name,
             state_text(s_items[ITEM_TOUCH].state),
             s_items[ITEM_TOUCH].detail,
             (long)x,
             (long)y);
}
