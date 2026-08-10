/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"

#include "s31_board_support.h"
#include "s31_test.h"

static bool s_imu_monitor_started;
static s31_imu_handle_t *s_imu;

static void imu_update_detail(const s31_imu_data_t *data)
{
    snprintf(s_items[ITEM_IMU].detail, sizeof(s_items[ITEM_IMU].detail),
             "acc %.2f %.2f %.2f gyr %.1f %.1f %.1f",
             data->accel_x_g,
             data->accel_y_g,
             data->accel_z_g,
             data->gyro_x_dps,
             data->gyro_y_dps,
             data->gyro_z_dps);
    s_items[ITEM_IMU].state = TEST_PASS;
    ui_refresh_item(ITEM_IMU);
}

static void imu_monitor_task(void *arg)
{
    (void)arg;

    while (true) {
        s31_imu_data_t data = {0};
        if (s31_imu_read(s_imu, &data) == ESP_OK) {
            imu_update_detail(&data);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

esp_err_t test_imu(void)
{
    esp_err_t ret = s31_imu_acquire(&s_imu);
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "QMI8658 init failed");

    uint8_t who = 0;
    uint8_t revision = 0;
    ret = s31_imu_get_identity(s_imu, &who, &revision);
    if (ret != ESP_OK) {
        goto cleanup;
    }

    bool ready = false;
    uint8_t status_int = 0;
    uint8_t status0 = 0;
    for (int i = 0; i < 20; i++) {
        ret = s31_imu_is_data_ready(s_imu, &ready, &status_int, &status0);
        if (ret != ESP_OK || ready) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (ret != ESP_OK) {
        goto cleanup;
    }
    if (!ready) {
        ret = ESP_ERR_TIMEOUT;
        goto cleanup;
    }

    s31_imu_data_t data = {0};
    ret = s31_imu_read(s_imu, &data);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    float accel_norm2 = data.accel_x_g * data.accel_x_g +
                        data.accel_y_g * data.accel_y_g +
                        data.accel_z_g * data.accel_z_g;
    if (accel_norm2 <= 0.01f || accel_norm2 >= 10.0f) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    set_item_result(ITEM_IMU, TEST_PASS,
                    "who=0x%02x rev=0x%02x acc=%.2f %.2f %.2f st=%02x/%02x gpio=%d",
                    (unsigned int)who, (unsigned int)revision,
                    data.accel_x_g, data.accel_y_g, data.accel_z_g,
                    data.status_int, data.status0, data.int_gpio_level);
    if (!s_imu_monitor_started) {
        BaseType_t created = xTaskCreate(imu_monitor_task, "imu_monitor", 4096, NULL, 5, NULL);
        if (created != pdPASS) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        s_imu_monitor_started = true;
    }
    return ESP_OK;

cleanup:
    s31_imu_release();
    s_imu = NULL;
    return ret;
}
