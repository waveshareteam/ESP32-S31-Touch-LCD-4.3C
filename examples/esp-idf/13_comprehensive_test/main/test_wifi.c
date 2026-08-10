/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "s31_test.h"

esp_err_t test_wifi_scan(void)
{
#if CONFIG_S31_TEST_ENABLE_WIFI_SCAN
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), S31_TEST_TAG, "erase NVS failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "NVS init failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), S31_TEST_TAG, "esp_netif init failed");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), S31_TEST_TAG, "WiFi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), S31_TEST_TAG, "WiFi storage config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), S31_TEST_TAG, "WiFi mode config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), S31_TEST_TAG, "WiFi start failed");

    wifi_scan_config_t scan_config = {
        .show_hidden = true,
    };
    ret = esp_wifi_scan_start(&scan_config, true);
    uint16_t ap_count = 0;
    if (ret == ESP_OK) {
        ret = esp_wifi_scan_get_ap_num(&ap_count);
    }

    esp_wifi_stop();
    esp_wifi_deinit();
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "WiFi scan failed");

    set_item_result(ITEM_WIFI, TEST_PASS, "scan ok, ap=%u", (unsigned int)ap_count);
    return ESP_OK;
#else
    set_item_result(ITEM_WIFI, TEST_SKIP, "disabled by config");
    return ESP_OK;
#endif
}
