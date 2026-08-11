/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_check.h"

#include "esp_board_manager_includes.h"
#include "s31_test.h"

esp_err_t test_tf_card(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name("fs_sdcard");
    if (ret != ESP_OK) {
        esp_board_manager_deinit_device_by_name("sdcard_power_ctrl");
        ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "mount TF card failed");
    }

    dev_fs_fat_handle_t *sdcard = NULL;
    ret = esp_board_manager_get_device_handle("fs_sdcard", (void **)&sdcard);
    if (ret != ESP_OK || !sdcard || !sdcard->mount_point) {
        ret = ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
        goto cleanup;
    }

    char file_path[128] = {0};
    int len = snprintf(file_path, sizeof(file_path), "%s/s31_comprehensive_test.txt",
                       sdcard->mount_point);
    ret = (len > 0 && len < (int)sizeof(file_path)) ? ESP_OK : ESP_ERR_INVALID_SIZE;

    const char *payload = "ESP32-S31 comprehensive TF card test OK\n";
    if (ret == ESP_OK) {
        FILE *file = fopen(file_path, "w");
        if (!file) {
            ret = ESP_FAIL;
        } else {
            int write_ret = fputs(payload, file);
            int close_ret = fclose(file);
            if (write_ret == EOF || close_ret != 0) {
                ret = ESP_FAIL;
            }
        }
    }

    char readback[80] = {0};
    if (ret == ESP_OK) {
        FILE *file = fopen(file_path, "r");
        if (!file) {
            ret = ESP_FAIL;
        } else {
            char *line = fgets(readback, sizeof(readback), file);
            fclose(file);
            if (!line || strcmp(readback, payload) != 0) {
                ret = ESP_FAIL;
            }
        }
    }

cleanup:
    {
        esp_err_t release_ret = esp_board_manager_deinit_device_by_name("fs_sdcard");
        if (ret == ESP_OK) {
            ret = release_ret;
        }
    }
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "TF card read/write verify failed");
    set_item_result(ITEM_TF_CARD, TEST_PASS, "read/write ok");
    return ESP_OK;
}
