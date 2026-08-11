/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_check.h"

#include "s31_board_support.h"
#include "s31_test.h"

esp_err_t test_bq27220(void)
{
    s31_battery_handle_t *battery = NULL;
    ESP_RETURN_ON_ERROR(s31_battery_acquire(&battery), S31_TEST_TAG, "BQ27220 init failed");

    s31_battery_info_t info = {0};
    esp_err_t ret = s31_battery_get_info(battery, &info);
    esp_err_t release_ret = s31_battery_release();
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "read BQ27220 info failed");
    ESP_RETURN_ON_ERROR(release_ret, S31_TEST_TAG, "release BQ27220 failed");
    ESP_RETURN_ON_FALSE(info.voltage_mv >= 2500 && info.voltage_mv <= 5000, ESP_FAIL,
                        S31_TEST_TAG, "BQ27220 voltage out of range: %umV",
                        (unsigned int)info.voltage_mv);

    set_item_result(ITEM_BQ27220, TEST_PASS, "%umV %dmA soc=%u%% soh=%u%%",
                    (unsigned int)info.voltage_mv,
                    info.current_ma,
                    (unsigned int)info.state_of_charge_percent,
                    (unsigned int)info.state_of_health_percent);
    return ESP_OK;
}
