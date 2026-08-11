/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"

#include "s31_board_support.h"
#include "s31_test.h"

static bool rtc_time_changed(const s31_rtc_time_t *a, const s31_rtc_time_t *b)
{
    return a->second != b->second || a->minute != b->minute || a->hour != b->hour ||
           a->day != b->day || a->month != b->month || a->year != b->year;
}

esp_err_t test_rtc(void)
{
    s31_rtc_handle_t *rtc = NULL;
    esp_err_t ret = s31_rtc_acquire(&rtc);
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "RTC init failed");

    s31_rtc_time_t before = {0};
    ret = s31_rtc_get_time(rtc, &before);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    if (before.voltage_low || before.year < 2024 || before.month == 0 || before.day == 0) {
        const s31_rtc_time_t initial_time = {
            .year = 2026,
            .month = 1,
            .day = 1,
            .weekday = 4,
            .hour = 12,
            .minute = 0,
            .second = 0,
        };
        ret = s31_rtc_set_time(rtc, &initial_time);
        if (ret == ESP_OK) {
            ret = s31_rtc_get_time(rtc, &before);
        }
        if (ret != ESP_OK) {
            goto cleanup;
        }
    }

    esp_err_t int_ret = s31_rtc_start_1hz_interrupt(rtc);
    s31_rtc_interrupt_status_t before_status = {0};
    uint32_t int_before = 0;
    if (int_ret == ESP_OK &&
        s31_rtc_get_interrupt_status(rtc, &before_status) == ESP_OK) {
        int_before = before_status.count;
    }
    vTaskDelay(pdMS_TO_TICKS(1500));

    s31_rtc_time_t after = {0};
    ret = s31_rtc_get_time(rtc, &after);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    s31_rtc_interrupt_status_t status = {0};
    esp_err_t status_ret = ESP_FAIL;
    if (int_ret == ESP_OK) {
        status_ret = s31_rtc_get_interrupt_status(rtc, &status);
        if (status_ret == ESP_OK) {
            s31_rtc_clear_interrupt(rtc);
        }
    }
    if (!rtc_time_changed(&before, &after)) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    if (int_ret == ESP_OK && status_ret == ESP_OK) {
        set_item_result(ITEM_RTC, TEST_PASS, "%04u-%02u-%02u %02u:%02u:%02u int=%lu gpio=%d",
                        (unsigned int)after.year,
                        (unsigned int)after.month,
                        (unsigned int)after.day,
                        (unsigned int)after.hour,
                        (unsigned int)after.minute,
                        (unsigned int)after.second,
                        (unsigned long)(status.count - int_before),
                        status.gpio_level);
    } else {
        set_item_result(ITEM_RTC, TEST_PASS, "%04u-%02u-%02u %02u:%02u:%02u int=n/a",
                        (unsigned int)after.year,
                        (unsigned int)after.month,
                        (unsigned int)after.day,
                        (unsigned int)after.hour,
                        (unsigned int)after.minute,
                        (unsigned int)after.second);
    }

cleanup:
    {
        esp_err_t release_ret = s31_rtc_release();
        return ret != ESP_OK ? ret : release_ret;
    }
}
